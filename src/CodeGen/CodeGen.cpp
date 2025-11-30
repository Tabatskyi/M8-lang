#include "CodeGen.hpp"

#include <iomanip>

using std::string;

namespace
{
constexpr std::size_t kFmtWriteI32Len = 4;
constexpr std::size_t kFmtWriteI64Len = 6;
constexpr std::size_t kFmtWriteStringLen = 4;
constexpr std::size_t kFmtReadI32Len = 3;
constexpr std::size_t kFmtReadI64Len = 5;
constexpr std::size_t kFmtReadStringLen = 7;
constexpr std::size_t kStringReadBufferBytes = 1024;

std::string encodeStringLiteral(const std::string& value)
{
	std::ostringstream oss;
	oss << "c\"";
	for (unsigned char ch : value)
	{
		switch (ch)
		{
		case '\n': oss << "\\0A"; break;
		case '\t': oss << "\\09"; break;
		case '\r': oss << "\\0D"; break;
		case '\"': oss << "\\22"; break;
		case '\\': oss << "\\5C"; break;
		default:
			if (ch >= 32 && ch <= 126)
			{
				oss << static_cast<char>(ch);
			}
			else
			{
				std::ostringstream hex;
				hex << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
				oss << "\\" << hex.str();
			}
			break;
		}
	}
	oss << "\\00\"";
	return oss.str();
}
}

CodeGenerator::CodeGenerator(IRContext& ctx,
							 const std::unordered_map<SymbolID, VariableInfo>& symbols,
							 const StructTable& structs,
							 const FunctionTable& functions)
	: _ctx(ctx), _structs(structs), _functions(functions)
{
	for (const auto& [id, info] : symbols)
	{
		CodegenVariable var;
		var.type = info.type;
		var.isMutable = info.isMutable;
		var.pointer = "%" + info.name + "." + std::to_string(id);
		_variables.emplace(id, std::move(var));
	}
}

void CodeGenerator::emitTopLevel(const ProgramNode& program)
{
	_emittingTopLevel = true;
	_emittedStructs.clear();

	for (const auto& stmt : program.statements())
	{
		if (const auto* structDecl = dynamic_cast<const StructDeclNode*>(stmt.get()))
			visitStructDecl(*structDecl);
	}

	for (const auto& stmt : program.statements())
	{
		if (const auto* func = dynamic_cast<const FunctionNode*>(stmt.get()))
			visitFunction(*func);
	}

	_emittingTopLevel = false;
	_currentBlockTerminated = false;
}

void CodeGenerator::generate(const ProgramNode& program)
{
	_currentBlockTerminated = false;
	program.accept(*this);
	if (!_currentBlockTerminated)
		emitInstruction("ret i32 0");
	_currentBlockTerminated = true;
}

void CodeGenerator::visitProgram(const ProgramNode& node)
{
	for (const auto& stmt : node.statements())
	{
		if (_currentBlockTerminated)
			break;
		if (!stmt)
			continue;
		if (dynamic_cast<const FunctionNode*>(stmt.get()) || dynamic_cast<const StructDeclNode*>(stmt.get()))
			continue;
		stmt->accept(*this);
	}
}

void CodeGenerator::visitBlock(const BlockNode& node)
{
	generateBlock(node, "");
}

void CodeGenerator::visitFunction(const FunctionNode& node)
{
	if (_emittingTopLevel)
		generateFunction(node);
}

void CodeGenerator::visitStructDecl(const StructDeclNode& node)
{
	if (_emittingTopLevel)
		emitStructDefinition(node);
}

void CodeGenerator::generateFunction(const FunctionNode& func)
{
	std::string retTypeIR;
	if (func.returnType().kind == TypeDesc::Kind::Builtin)
		retTypeIR = llvmType(func.returnType().builtin);
	else
		retTypeIR = "%struct." + func.returnType().structName;

	_ctx.ir << "define " << retTypeIR << " @" << func.name() << "(";
	for (size_t i = 0; i < func.params().size(); ++i)
	{
		if (i > 0)
			_ctx.ir << ", ";
		const auto& param = func.params()[i];
		if (param.type.kind == TypeDesc::Kind::Builtin)
			_ctx.ir << llvmType(param.type.builtin) << " %" << param.name;
		else
			_ctx.ir << "%struct." << param.type.structName << " %" << param.name;
	}
	_ctx.ir << ") {\n";

	const FunctionInfo* funcInfo = nullptr;
	auto fit = _functions.find(func.name());
	if (fit != _functions.end())
		funcInfo = &fit->second;

	if (funcInfo)
	{
		for (size_t i = 0; i < funcInfo->params.size(); ++i)
		{
			const FunctionParamInfo& paramInfo = funcInfo->params[i];
			if (paramInfo.symbolId == InvalidSymbolID)
				continue;
			CodegenVariable& var = getVariable(paramInfo.symbolId);
			ensureAllocated(var);
			const std::string source = "%" + paramInfo.name;
			if (paramInfo.type.kind == TypeDesc::Kind::Builtin)
				emitInstruction("store " + llvmType(paramInfo.type.builtin) + " " + source + ", " + llvmType(paramInfo.type.builtin) + "* " + var.pointer);
			else
				emitInstruction("store %struct." + paramInfo.type.structName + " " + source + ", %struct." + paramInfo.type.structName + "* " + var.pointer);
			var.initialized = true;
		}
	}
	else
	{
		for (const auto& param : func.params())
		{
			if (param.symbolId == InvalidSymbolID)
				continue;
			CodegenVariable& var = getVariable(param.symbolId);
			ensureAllocated(var);
			if (param.type.kind == TypeDesc::Kind::Builtin)
				emitInstruction("store " + llvmType(param.type.builtin) + " %" + param.name + ", " + llvmType(param.type.builtin) + "* " + var.pointer);
			else
				emitInstruction("store %struct." + param.type.structName + " %" + param.name + ", %struct." + param.type.structName + "* " + var.pointer);
			var.initialized = true;
		}
	}

	bool previousInFunction = _inFunction;
	TypeDesc previousReturn = _functionReturnType;
	_inFunction = true;
	_functionReturnType = func.returnType();
	_currentMemberMaster = func.isMember() ? func.masterStruct() : std::string{};
	_currentMemberFunctionName = func.name();
	_selfSymbolId = InvalidSymbolID;
	_currentBlockTerminated = false;

	bool fallsThrough = true;
	if (func.body())
		fallsThrough = generateBlock(*func.body(), "");
	if (fallsThrough)
	{
		if (func.returnType().kind == TypeDesc::Kind::Builtin)
			emitInstruction("ret " + llvmType(func.returnType().builtin) + " 0");
		else
			emitInstruction("ret %struct." + func.returnType().structName + " zeroinitializer");
	}

	_ctx.ir << "}\n";

	_inFunction = previousInFunction;
	_functionReturnType = previousReturn;
	_currentMemberMaster.clear();
	_currentMemberFunctionName.clear();
	_currentBlockTerminated = false;
}

void CodeGenerator::visitDecl(const DeclNode& node)
{
	SymbolID symbolId = node.symbolId();
	if (symbolId == InvalidSymbolID)
		return;

	CodegenVariable& var = getVariable(symbolId);
	ensureAllocated(var);

	if (var.type.kind == TypeDesc::Kind::Builtin)
	{
		CodegenValue value{ "0", false, var.type.builtin, "" };
		if (node.hasInitializer() && !node.initializers().empty() && node.initializers().front())
		{
			const ExprNode* init = node.initializers().front().get();
			init->accept(*this);
			value = popValue();
			value = ensureType(std::move(value), var.type.builtin);
		}
		storeValue(var, value);
		return;
	}

	auto structIt = _structs.find(var.type.structName);
	if (!node.hasInitializer() || node.initializers().empty() || structIt == _structs.end())
	{
		emitInstruction("store %struct." + var.type.structName + " zeroinitializer, %struct." + var.type.structName + "* " + var.pointer);
		var.initialized = true;
		return;
	}

	const StructInfo& structInfo = structIt->second;
	size_t initCount = std::min(node.initializers().size(), structInfo.fields.size());
	for (size_t i = 0; i < initCount; ++i)
	{
		const StructFieldInfo& field = structInfo.fields[i];
		std::string fieldPtr = nextTemp();
		emitInstruction(fieldPtr + " = getelementptr %struct." + structInfo.name + ", %struct." + structInfo.name + "* " + var.pointer + ", i32 0, i32 " + std::to_string(i));

		const ExprNode* expr = node.initializers()[i].get();
		if (!expr)
			continue;
		expr->accept(*this);
		CodegenValue val = popValue();
		if (field.type.kind == TypeDesc::Kind::Builtin)
		{
			val = ensureType(std::move(val), field.type.builtin);
			emitInstruction("store " + llvmType(field.type.builtin) + " " + val.operand + ", " + llvmType(field.type.builtin) + "* " + fieldPtr);
		}
		else
		{
			if (!val.isStruct)
			{
				std::string zeroTmp = nextTemp();
				emitInstruction(zeroTmp + " = insertvalue %struct." + field.type.structName + " undef, i32 0, 0");
				val = { zeroTmp, true, ValueType::Invalid, field.type.structName };
			}
			emitInstruction("store %struct." + field.type.structName + " " + val.operand + ", %struct." + field.type.structName + "* " + fieldPtr);
		}
	}
	var.initialized = true;
}

void CodeGenerator::visitAssign(const AssignNode& node)
{
	SymbolID symbolId = node.symbolId();
	if (symbolId == InvalidSymbolID)
		return;

	CodegenVariable& var = getVariable(symbolId);
	ensureAllocated(var);

	if (const ExprNode* valueExpr = node.value())
	{
		valueExpr->accept(*this);
		CodegenValue value = popValue();
		if (var.type.kind == TypeDesc::Kind::Builtin)
			value = ensureType(std::move(value), var.type.builtin);
		storeValue(var, value);
	}
}

void CodeGenerator::visitAssignField(const AssignFieldNode& node)
{
	const FieldAccessNode* target = node.target();
	if (!target)
		return;

	std::string fieldPtr = getFieldPointer(*target);
	if (fieldPtr.empty())
		return;

	if (const ExprNode* valueExpr = node.value())
	{
		valueExpr->accept(*this);
		CodegenValue value = popValue();
		TypeDesc fieldType = fieldTypeDesc(*target);
		if (fieldType.kind == TypeDesc::Kind::Builtin)
		{
			value = ensureType(std::move(value), fieldType.builtin);
			emitInstruction("store " + llvmType(fieldType.builtin) + " " + value.operand + ", " + llvmType(fieldType.builtin) + "* " + fieldPtr);
		}
		else
		{
			if (!value.isStruct)
			{
				std::string zeroTmp = nextTemp();
				emitInstruction(zeroTmp + " = insertvalue %struct." + fieldType.structName + " undef, i32 0, 0");
				value = { zeroTmp, true, ValueType::Invalid, fieldType.structName };
			}
			emitInstruction("store %struct." + fieldType.structName + " " + value.operand + ", %struct." + fieldType.structName + "* " + fieldPtr);
		}
	}
}

void CodeGenerator::visitFieldAccess(const FieldAccessNode& node)
{
	std::string fieldPtr = getFieldPointer(node);
	if (fieldPtr.empty())
	{
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	TypeDesc fieldType = fieldTypeDesc(node);
	if (fieldType.kind == TypeDesc::Kind::Builtin)
	{
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = load " + llvmType(fieldType.builtin) + ", " + llvmType(fieldType.builtin) + "* " + fieldPtr);
		pushValue({ tmp, false, fieldType.builtin, "" });
	}
	else
	{
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = load %struct." + fieldType.structName + ", %struct." + fieldType.structName + "* " + fieldPtr);
		pushValue({ tmp, true, ValueType::Invalid, fieldType.structName });
	}
}

void CodeGenerator::visitFunctionCall(const FunctionCallNode& node)
{
	if (node.symbolId() != InvalidSymbolID)
	{
		CodegenVariable& var = getVariable(node.symbolId());
		ensureAllocated(var);
		if (var.type.kind != TypeDesc::Kind::Struct)
		{
			pushValue({ "0", false, ValueType::Invalid, "" });
			return;
		}

		const FunctionInfo* callInfo = findMemberFunction("call", var.type.structName);
		if (!callInfo)
		{
			pushValue({ "0", false, ValueType::Invalid, "" });
			return;
		}

		std::vector<CodegenValue> args;
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = load %struct." + var.type.structName + ", %struct." + var.type.structName + "* " + var.pointer);
		args.push_back({ tmp, true, ValueType::Invalid, var.type.structName });

		for (size_t i = 0; i < node.args().size(); ++i)
		{
			const ExprNode* expr = node.args()[i].get();
			if (!expr)
				continue;
			expr->accept(*this);
			CodegenValue argVal = popValue();
			if (i + 1 < callInfo->params.size() && callInfo->params[i + 1].type.kind == TypeDesc::Kind::Builtin)
				argVal = ensureType(std::move(argVal), callInfo->params[i + 1].type.builtin);
			args.push_back(std::move(argVal));
		}

		std::string retTypeIR = (callInfo->returnType.kind == TypeDesc::Kind::Struct)
									? "%struct." + callInfo->returnType.structName
									: llvmType(callInfo->returnType.builtin);
		std::string callTmp = nextTemp();
		std::ostringstream argStream;
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (i > 0)
				argStream << ", ";
			const FunctionParamInfo& formal = callInfo->params[i];
			if (formal.type.kind == TypeDesc::Kind::Struct)
				argStream << "%struct." << formal.type.structName << " " << args[i].operand;
			else
				argStream << llvmType(formal.type.builtin) << " " << args[i].operand;
		}
		emitInstruction(callTmp + " = call " + retTypeIR + " @" + callInfo->name + "(" + argStream.str() + ")");
		if (callInfo->returnType.kind == TypeDesc::Kind::Struct)
			pushValue({ callTmp, true, ValueType::Invalid, callInfo->returnType.structName });
		else
			pushValue({ callTmp, false, callInfo->returnType.builtin, "" });
		return;
	}

	auto fit = _functions.find(node.name());
	if (fit == _functions.end())
	{
		for (const auto& arg : node.args())
		{
			if (!arg)
				continue;
			arg->accept(*this);
			popValue();
		}
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	const FunctionInfo& function = fit->second;
	if (function.isBuiltin)
	{
		if (handleBuiltinFunctionCall(node, function))
			return;
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	std::vector<CodegenValue> args;
	for (size_t i = 0; i < node.args().size(); ++i)
	{
		const ExprNode* expr = node.args()[i].get();
		if (!expr)
			continue;
		expr->accept(*this);
		CodegenValue argVal = popValue();
		if (i < function.params.size() && function.params[i].type.kind == TypeDesc::Kind::Builtin)
			argVal = ensureType(std::move(argVal), function.params[i].type.builtin);
		args.push_back(std::move(argVal));
	}

	std::string retTypeIR = (function.returnType.kind == TypeDesc::Kind::Struct)
								? "%struct." + function.returnType.structName
								: llvmType(function.returnType.builtin);
	std::string callTmp = nextTemp();
	std::ostringstream argStream;
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (i > 0)
			argStream << ", ";
		if (i < function.params.size() && function.params[i].type.kind == TypeDesc::Kind::Struct)
			argStream << "%struct." << function.params[i].type.structName << " " << args[i].operand;
		else if (i < function.params.size() && function.params[i].type.kind == TypeDesc::Kind::Builtin)
			argStream << llvmType(function.params[i].type.builtin) << " " << args[i].operand;
		else if (args[i].isStruct)
			argStream << "%struct." << args[i].structName << " " << args[i].operand;
		else
			argStream << llvmType(args[i].type) << " " << args[i].operand;
	}
	emitInstruction(callTmp + " = call " + retTypeIR + " @" + node.name() + "(" + argStream.str() + ")");
	if (function.returnType.kind == TypeDesc::Kind::Struct)
		pushValue({ callTmp, true, ValueType::Invalid, function.returnType.structName });
	else
		pushValue({ callTmp, false, function.returnType.builtin, "" });
}

void CodeGenerator::visitMemberFunctionCall(const MemberFunctionCallNode& node)
{
	SymbolID baseId = node.baseSymbolId();
	if (baseId == InvalidSymbolID)
	{
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	CodegenVariable& baseVar = getVariable(baseId);
	ensureAllocated(baseVar);
	if (baseVar.type.kind != TypeDesc::Kind::Struct)
	{
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	TypeDesc current = baseVar.type;
	std::string basePtr = baseVar.pointer;
	for (const std::string& fieldName : node.fieldChain())
	{
		auto structIt = _structs.find(current.structName);
		if (structIt == _structs.end())
		{
			pushValue({ "0", false, ValueType::Invalid, "" });
			return;
		}

		int idx = -1;
		TypeDesc nextType = TypeDesc::Builtin(ValueType::Invalid);
		for (size_t i = 0; i < structIt->second.fields.size(); ++i)
		{
			if (structIt->second.fields[i].name == fieldName)
			{
				idx = static_cast<int>(i);
				nextType = structIt->second.fields[i].type;
				break;
			}
		}

		if (idx < 0)
		{
			pushValue({ "0", false, ValueType::Invalid, "" });
			return;
		}

		std::string gep = nextTemp();
		emitInstruction(gep + " = getelementptr %struct." + structIt->second.name + ", %struct." + structIt->second.name + "* " + basePtr + ", i32 0, i32 " + std::to_string(idx));
		basePtr = gep;
		current = nextType;
	}

	if (current.kind != TypeDesc::Kind::Struct)
	{
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	std::string structVal = nextTemp();
	emitInstruction(structVal + " = load %struct." + current.structName + ", %struct." + current.structName + "* " + basePtr);

	const FunctionInfo* funcInfo = findMemberFunction(node.funcName(), current.structName);
	if (!funcInfo)
	{
		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	std::vector<CodegenValue> args;
	args.push_back({ structVal, true, ValueType::Invalid, current.structName });

	for (size_t i = 0; i < node.args().size(); ++i)
	{
		const ExprNode* expr = node.args()[i].get();
		if (!expr)
			continue;
		expr->accept(*this);
		CodegenValue argVal = popValue();
		if (i + 1 < funcInfo->params.size() && funcInfo->params[i + 1].type.kind == TypeDesc::Kind::Builtin)
			argVal = ensureType(std::move(argVal), funcInfo->params[i + 1].type.builtin);
		args.push_back(std::move(argVal));
	}

	std::string retTypeIR = (funcInfo->returnType.kind == TypeDesc::Kind::Struct)
								? "%struct." + funcInfo->returnType.structName
								: llvmType(funcInfo->returnType.builtin);
	std::string callTmp = nextTemp();
	std::ostringstream argStream;
	for (size_t i = 0; i < args.size(); ++i)
	{
		if (i > 0)
			argStream << ", ";
		const FunctionParamInfo& formal = funcInfo->params[i];
		if (formal.type.kind == TypeDesc::Kind::Struct)
			argStream << "%struct." << formal.type.structName << " " << args[i].operand;
		else
			argStream << llvmType(formal.type.builtin) << " " << args[i].operand;
	}
	emitInstruction(callTmp + " = call " + retTypeIR + " @" + node.funcName() + "(" + argStream.str() + ")");
	if (funcInfo->returnType.kind == TypeDesc::Kind::Struct)
		pushValue({ callTmp, true, ValueType::Invalid, funcInfo->returnType.structName });
	else
		pushValue({ callTmp, false, funcInfo->returnType.builtin, "" });
}

void CodeGenerator::visitIf(const IfNode& node)
{
	if (!node.condition() || !node.thenBlock())
		return;

	node.condition()->accept(*this);
	CodegenValue cond = popValue();
	cond = ensureType(std::move(cond), ValueType::Bool);

	string thenLabel = nextLabel("then");
	string endLabel = nextLabel("endif");
	bool hasElse = node.elseBlock() != nullptr;
	string elseLabel = hasElse ? nextLabel("else") : "";
	string falseLabel = hasElse ? elseLabel : endLabel;

	emitInstruction("br i1 " + cond.operand + ", label %" + thenLabel + ", label %" + falseLabel);
	_currentBlockTerminated = true;

	emitLabel(thenLabel);
	bool thenFallsThrough = generateBlock(*node.thenBlock(), endLabel);

	bool elseFallsThrough = false;
	if (hasElse)
	{
		emitLabel(elseLabel);
		elseFallsThrough = generateBlock(*node.elseBlock(), endLabel);
	}

	if (!hasElse || thenFallsThrough || elseFallsThrough)
		emitLabel(endLabel);

	if (hasElse && !thenFallsThrough && !elseFallsThrough)
		_currentBlockTerminated = true;
	else
		_currentBlockTerminated = false;
}

void CodeGenerator::visitReturn(const ReturnNode& node)
{
	if (!node.expr())
		return;
	node.expr()->accept(*this);
	CodegenValue value = popValue();
	emitReturn(std::move(value));
}

void CodeGenerator::visitBinaryOp(const BinaryOpNode& node)
{
	if (const ExprNode* left = node.left())
		left->accept(*this);
	if (const ExprNode* right = node.right())
		right->accept(*this);

	CodegenValue rightVal = popValue();
	CodegenValue leftVal = popValue();

	switch (node.op())
	{
	case BinaryOpNode::Operator::Add:
	case BinaryOpNode::Operator::Sub:
	case BinaryOpNode::Operator::Mul:
	case BinaryOpNode::Operator::Div:
	{
		ValueType targetType = node.type();
		leftVal = ensureType(std::move(leftVal), targetType);
		rightVal = ensureType(std::move(rightVal), targetType);

		std::string opInstr;
		switch (node.op())
		{
		case BinaryOpNode::Operator::Add: opInstr = "add"; break;
		case BinaryOpNode::Operator::Sub: opInstr = "sub"; break;
		case BinaryOpNode::Operator::Mul: opInstr = "mul"; break;
		case BinaryOpNode::Operator::Div: opInstr = "sdiv"; break;
		default: opInstr = "add"; break;
		}

		std::string tmp = nextTemp();
		emitInstruction(tmp + " = " + opInstr + " " + llvmType(targetType) + " " + leftVal.operand + ", " + rightVal.operand);
		pushValue({ tmp, false, targetType, "" });
		return;
	}
	case BinaryOpNode::Operator::Equal:
	case BinaryOpNode::Operator::NotEqual:
	{
		ValueType operandType = comparisonOperandType(leftVal.type, rightVal.type);
		leftVal = ensureType(std::move(leftVal), operandType);
		rightVal = ensureType(std::move(rightVal), operandType);
		std::string cmp = (node.op() == BinaryOpNode::Operator::Equal) ? "icmp eq" : "icmp ne";
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = " + cmp + " " + llvmType(operandType) + " " + leftVal.operand + ", " + rightVal.operand);
		pushValue({ tmp, false, ValueType::Bool, "" });
		return;
	}
	}

	pushValue({ "0", false, ValueType::Invalid, "" });
}

void CodeGenerator::visitUnaryOp(const UnaryOpNode& node)
{
	if (const ExprNode* operand = node.operand())
		operand->accept(*this);

	CodegenValue value = popValue();
	value = ensureType(std::move(value), ValueType::Bool);
	std::string tmp = nextTemp();
	emitInstruction(tmp + " = xor i1 " + value.operand + ", 1");
	pushValue({ tmp, false, ValueType::Bool, "" });
}

void CodeGenerator::visitID(const IDNode& node)
{
	SymbolID sid = node.symbolId();
	if (sid == InvalidSymbolID)
	{
		if (!_currentMemberMaster.empty())
		{
			if (_selfSymbolId == InvalidSymbolID)
			{
				auto fit = _functions.find(_currentMemberFunctionName);
				if (fit != _functions.end())
				{
					for (const auto& param : fit->second.params)
					{
						if (param.name == "_self")
						{
							_selfSymbolId = param.symbolId;
							break;
						}
					}
				}
			}

			if (_selfSymbolId != InvalidSymbolID)
			{
				auto structIt = _structs.find(_currentMemberMaster);
				if (structIt != _structs.end())
				{
					int idx = -1;
					TypeDesc fieldType = TypeDesc::Builtin(ValueType::Invalid);
					for (size_t i = 0; i < structIt->second.fields.size(); ++i)
					{
						if (structIt->second.fields[i].name == node.name())
						{
							idx = static_cast<int>(i);
							fieldType = structIt->second.fields[i].type;
							break;
						}
					}

					if (idx >= 0 && fieldType.kind == TypeDesc::Kind::Builtin)
					{
						CodegenVariable& selfVar = getVariable(_selfSymbolId);
						ensureAllocated(selfVar);
						std::string gep = nextTemp();
						emitInstruction(gep + " = getelementptr %struct." + _currentMemberMaster + ", %struct." + _currentMemberMaster + "* " + selfVar.pointer + ", i32 0, i32 " + std::to_string(idx));
						std::string tmp = nextTemp();
						emitInstruction(tmp + " = load " + llvmType(fieldType.builtin) + ", " + llvmType(fieldType.builtin) + "* " + gep);
						pushValue({ tmp, false, fieldType.builtin, "" });
						return;
					}
				}
			}
		}

		pushValue({ "0", false, ValueType::Invalid, "" });
		return;
	}

	CodegenVariable& var = getVariable(sid);
	ensureAllocated(var);
	if (!var.initialized)
	{
		if (var.type.kind == TypeDesc::Kind::Builtin)
			storeValue(var, { zeroLiteral(var.type.builtin), false, var.type.builtin, "" });
		else
			emitInstruction("store %struct." + var.type.structName + " zeroinitializer, %struct." + var.type.structName + "* " + var.pointer);
		var.initialized = true;
	}

	std::string tmp = nextTemp();
	if (var.type.kind == TypeDesc::Kind::Builtin)
	{
		emitInstruction(tmp + " = load " + llvmType(var.type.builtin) + ", " + llvmType(var.type.builtin) + "* " + var.pointer);
		pushValue({ tmp, false, var.type.builtin, "" });
	}
	else
	{
		emitInstruction(tmp + " = load %struct." + var.type.structName + ", %struct." + var.type.structName + "* " + var.pointer);
		pushValue({ tmp, true, ValueType::Invalid, var.type.structName });
	}
}

void CodeGenerator::visitNumber(const NumberNode& node)
{
	std::int64_t value = node.value();
	CodegenValue out;
	if (value >= std::numeric_limits<std::int32_t>::min() && value <= std::numeric_limits<std::int32_t>::max())
	{
		out.type = ValueType::I32;
		out.operand = std::to_string(static_cast<std::int32_t>(value));
	}
	else
	{
		out.type = ValueType::I64;
		out.operand = std::to_string(value);
	}
	out.isStruct = false;
	out.structName.clear();
	pushValue(std::move(out));
}

void CodeGenerator::visitBoolLiteral(const BoolLiteralNode& node)
{
	pushValue({ node.value() ? "1" : "0", false, ValueType::Bool, "" });
}

void CodeGenerator::visitStringLiteral(const StringLiteralNode& node)
{
	const auto& info = internStringLiteral(node.value());
	std::string ptr = formatPointer(info.globalName, info.length);
	pushValue({ ptr, false, ValueType::String, "" });
}

void CodeGenerator::visitStructLiteral(const StructLiteralNode& node)
{
	if (node.structType().kind != TypeDesc::Kind::Struct)
	{
		pushValue({ "zeroinitializer", true, ValueType::Invalid, "" });
		return;
	}

	auto infoIt = _structs.find(node.structType().structName);
	if (infoIt == _structs.end())
	{
		pushValue({ "zeroinitializer", true, ValueType::Invalid, node.structType().structName });
		return;
	}

	const StructInfo& info = infoIt->second;
	std::string literalPtr = nextTemp();
	emitInstruction(literalPtr + " = alloca %struct." + info.name);
	emitInstruction("store %struct." + info.name + " zeroinitializer, %struct." + info.name + "* " + literalPtr);

	size_t limit = std::min(node.args().size(), info.fields.size());
	for (size_t i = 0; i < limit; ++i)
	{
		if (!node.args()[i])
			continue;
		node.args()[i]->accept(*this);
		CodegenValue val = popValue();
		const StructFieldInfo& field = info.fields[i];
		std::string fieldPtr = nextTemp();
		emitInstruction(fieldPtr + " = getelementptr %struct." + info.name + ", %struct." + info.name + "* " + literalPtr + ", i32 0, i32 " + std::to_string(i));
		if (field.type.kind == TypeDesc::Kind::Builtin)
		{
			val = ensureType(std::move(val), field.type.builtin);
			emitInstruction("store " + llvmType(field.type.builtin) + " " + val.operand + ", " + llvmType(field.type.builtin) + "* " + fieldPtr);
		}
		else
		{
			if (!val.isStruct || val.structName != field.type.structName)
			{
				std::string zeroTmp = nextTemp();
				emitInstruction(zeroTmp + " = insertvalue %struct." + field.type.structName + " undef, i32 0, 0");
				val = { zeroTmp, true, ValueType::Invalid, field.type.structName };
			}
			emitInstruction("store %struct." + field.type.structName + " " + val.operand + ", %struct." + field.type.structName + "* " + fieldPtr);
		}
	}

	std::string loaded = nextTemp();
	emitInstruction(loaded + " = load %struct." + info.name + ", %struct." + info.name + "* " + literalPtr);
	pushValue({ loaded, true, ValueType::Invalid, info.name });
}

const FunctionInfo* CodeGenerator::findMemberFunction(const std::string& funcName, const std::string& structName) const
{
	auto it = _functions.find(funcName);
	if (it != _functions.end() && it->second.isMember && it->second.masterStruct == structName)
		return &it->second;

	for (const auto& entry : _functions)
	{
		if (entry.second.name == funcName && entry.second.isMember && entry.second.masterStruct == structName)
			return &entry.second;
	}
	return nullptr;
}

std::string CodeGenerator::llvmType(ValueType type) const
{
	switch (type)
	{
	case ValueType::I32: return "i32";
	case ValueType::I64: return "i64";
	case ValueType::Bool: return "i1";
	case ValueType::String: return "i8*";
	default: return "i32";
	}
}

std::string CodeGenerator::zeroLiteral(ValueType type) const
{
	switch (type)
	{
	case ValueType::I32:
	case ValueType::I64:
	case ValueType::Bool:
		return "0";
	case ValueType::String:
		return "null";
	default:
		return "0";
	}
}

std::string CodeGenerator::nextTemp()
{
	return "%t" + std::to_string(_ctx.tempId++);
}

std::string CodeGenerator::nextLabel(const std::string& base)
{
	return base + std::to_string(_labelId++);
}

void CodeGenerator::emitLabel(const std::string& label)
{
	_ctx.ir << label << ":\n";
}

void CodeGenerator::emitInstruction(const std::string& text)
{
	_ctx.ir << "  " << text << "\n";
}

void CodeGenerator::pushValue(CodegenValue value)
{
	_stack.push_back(std::move(value));
}

CodegenValue CodeGenerator::popValue()
{
	if (_stack.empty())
		return { "0", false, ValueType::Invalid, "" };
	CodegenValue value = std::move(_stack.back());
	_stack.pop_back();
	return value;
}

TypeDesc CodeGenerator::fieldTypeDesc(const FieldAccessNode& node)
{
	SymbolID sid = node.baseSymbolId();
	if (sid == InvalidSymbolID)
		return TypeDesc::Builtin(ValueType::Invalid);

	auto varIt = _variables.find(sid);
	if (varIt == _variables.end())
		return TypeDesc::Builtin(ValueType::Invalid);

	TypeDesc current = varIt->second.type;
	for (const std::string& fieldName : node.fieldChain())
	{
		if (current.kind != TypeDesc::Kind::Struct)
			return TypeDesc::Builtin(ValueType::Invalid);
		auto structIt = _structs.find(current.structName);
		if (structIt == _structs.end())
			return TypeDesc::Builtin(ValueType::Invalid);
		bool found = false;
		for (const auto& field : structIt->second.fields)
		{
			if (field.name == fieldName)
			{
				current = field.type;
				found = true;
				break;
			}
		}
		if (!found)
			return TypeDesc::Builtin(ValueType::Invalid);
	}
	return current;
}

std::string CodeGenerator::getFieldPointer(const FieldAccessNode& node)
{
	SymbolID sid = node.baseSymbolId();
	if (sid == InvalidSymbolID)
		return {};

	CodegenVariable& base = getVariable(sid);
	ensureAllocated(base);
	if (base.type.kind != TypeDesc::Kind::Struct)
		return {};

	std::string ptr = base.pointer;
	TypeDesc current = base.type;
	for (const std::string& fieldName : node.fieldChain())
	{
		auto structIt = _structs.find(current.structName);
		if (structIt == _structs.end())
			return {};
		int idx = -1;
		TypeDesc nextType = TypeDesc::Builtin(ValueType::Invalid);
		for (size_t i = 0; i < structIt->second.fields.size(); ++i)
		{
			if (structIt->second.fields[i].name == fieldName)
			{
				idx = static_cast<int>(i);
				nextType = structIt->second.fields[i].type;
				break;
			}
		}
		if (idx < 0)
			return {};
		std::string gep = nextTemp();
		emitInstruction(gep + " = getelementptr %struct." + structIt->second.name + ", %struct." + structIt->second.name + "* " + ptr + ", i32 0, i32 " + std::to_string(idx));
		ptr = gep;
		current = nextType;
	}
	return ptr;
}

CodegenVariable& CodeGenerator::getVariable(SymbolID id)
{
	auto it = _variables.find(id);
	if (it == _variables.end())
		it = _variables.emplace(id, CodegenVariable{}).first;
	CodegenVariable& var = it->second;
	if (var.pointer.empty())
		var.pointer = "%tmpvar." + std::to_string(id);
	return var;
}

void CodeGenerator::ensureAllocated(CodegenVariable& var)
{
	if (!var.allocated && !var.pointer.empty())
	{
		if (var.type.kind == TypeDesc::Kind::Builtin)
			emitInstruction(var.pointer + " = alloca " + llvmType(var.type.builtin));
		else
			emitInstruction(var.pointer + " = alloca %struct." + var.type.structName);
		var.allocated = true;
	}
}

CodegenValue CodeGenerator::ensureType(CodegenValue value, ValueType target)
{
	if (value.isStruct)
		return value;
	if (target == ValueType::Invalid || value.type == ValueType::Invalid)
		return { value.operand, false, ValueType::Invalid, "" };

	if (value.type == target)
		return value;

	if (target == ValueType::I64 && value.type == ValueType::I32)
	{
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = sext i32 " + value.operand + " to i64");
		return { tmp, false, ValueType::I64, "" };
	}

	if (target == ValueType::I32 && value.type == ValueType::I64)
	{
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = trunc i64 " + value.operand + " to i32");
		return { tmp, false, ValueType::I32, "" };
	}

	if (target == ValueType::I32 && value.type == ValueType::Bool)
	{
		std::string tmp = nextTemp();
		emitInstruction(tmp + " = zext i1 " + value.operand + " to i32");
		return { tmp, false, ValueType::I32, "" };
	}

	if (target == ValueType::I64 && value.type == ValueType::Bool)
	{
		CodegenValue widened = ensureType(std::move(value), ValueType::I32);
		return ensureType(std::move(widened), ValueType::I64);
	}

	if (target == ValueType::Bool && value.type != ValueType::Bool)
		return { value.operand, false, ValueType::Invalid, "" };

	return value;
}

void CodeGenerator::storeValue(CodegenVariable& var, const CodegenValue& value)
{
	ensureAllocated(var);
	if (var.type.kind == TypeDesc::Kind::Builtin)
		emitInstruction("store " + llvmType(var.type.builtin) + " " + value.operand + ", " + llvmType(var.type.builtin) + "* " + var.pointer);
	else
		emitInstruction("store %struct." + var.type.structName + " " + value.operand + ", %struct." + var.type.structName + "* " + var.pointer);
	var.initialized = true;
}

void CodeGenerator::emitReturn(CodegenValue value)
{
	if (_inFunction)
	{
		if (_functionReturnType.kind == TypeDesc::Kind::Builtin)
		{
			value = ensureType(std::move(value), _functionReturnType.builtin);
			emitInstruction("ret " + llvmType(_functionReturnType.builtin) + " " + value.operand);
		}
		else
		{
			emitInstruction("ret %struct." + _functionReturnType.structName + " " + value.operand);
		}
	}
	else
	{
		value = ensureType(std::move(value), ValueType::I32);
		std::string fmtPtr = nextTemp();
		emitInstruction(fmtPtr + " = getelementptr [29 x i8], [29 x i8]* @fmt_exit, i32 0, i32 0");
		emitInstruction("call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + value.operand + ")");
		emitInstruction("ret i32 " + value.operand);
	}
	_currentBlockTerminated = true;
}

bool CodeGenerator::generateBlock(const BlockNode& node, const std::string& exitLabel)
{
	bool saved = _currentBlockTerminated;
	_currentBlockTerminated = false;

	for (const auto& stmt : node.statements())
	{
		if (_currentBlockTerminated)
			break;
		if (stmt)
			stmt->accept(*this);
	}

	bool fallsThrough = !_currentBlockTerminated;
	if (fallsThrough && !exitLabel.empty())
		emitInstruction("br label %" + exitLabel);

	_currentBlockTerminated = saved;
	return fallsThrough;
}

bool CodeGenerator::handleBuiltinFunctionCall(const FunctionCallNode& node, const FunctionInfo& info)
{
	if (info.name == "__builtin_write")
	{
		if (node.args().empty() || !node.args()[0])
		{
			pushValue({ "0", false, ValueType::I32, "" });
			return true;
		}

		node.args()[0]->accept(*this);
		CodegenValue value = popValue();
		ValueType argType = node.args()[0]->type();
		std::string callTmp;
		switch (argType)
		{
		case ValueType::I64:
		{
			value = ensureType(std::move(value), ValueType::I64);
			std::string fmtPtr = formatPointer("@fmt_write_i64", kFmtWriteI64Len);
			callTmp = nextTemp();
			emitInstruction(callTmp + " = call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i64 " + value.operand + ")");
			pushValue({ callTmp, false, ValueType::I32, "" });
			return true;
		}
		case ValueType::Bool:
		{
			value = ensureType(std::move(value), ValueType::Bool);
			std::string widened = nextTemp();
			emitInstruction(widened + " = zext i1 " + value.operand + " to i32");
			std::string fmtPtr = formatPointer("@fmt_write_i32", kFmtWriteI32Len);
			callTmp = nextTemp();
			emitInstruction(callTmp + " = call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + widened + ")");
			pushValue({ callTmp, false, ValueType::I32, "" });
			return true;
		}
		case ValueType::String:
		{
			value = ensureType(std::move(value), ValueType::String);
			std::string fmtPtr = formatPointer("@fmt_write_str", kFmtWriteStringLen);
			callTmp = nextTemp();
			emitInstruction(callTmp + " = call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i8* " + value.operand + ")");
			pushValue({ callTmp, false, ValueType::I32, "" });
			return true;
		}
		case ValueType::I32:
		default:
		{
			value = ensureType(std::move(value), ValueType::I32);
			std::string fmtPtr = formatPointer("@fmt_write_i32", kFmtWriteI32Len);
			callTmp = nextTemp();
			emitInstruction(callTmp + " = call i32 (i8*, ...) @printf(i8* " + fmtPtr + ", i32 " + value.operand + ")");
			pushValue({ callTmp, false, ValueType::I32, "" });
			return true;
		}
		}
	}

	if (info.name == "__builtin_read")
	{
		ValueType targetType = node.type();
		switch (targetType)
		{
		case ValueType::I64:
		{
			std::string slot = nextTemp();
			emitInstruction(slot + " = alloca i64");
			std::string fmtPtr = formatPointer("@fmt_read_i64", kFmtReadI64Len);
			emitInstruction("call i32 (i8*, ...) @scanf(i8* " + fmtPtr + ", i64* " + slot + ")");
			std::string loaded = nextTemp();
			emitInstruction(loaded + " = load i64, i64* " + slot);
			pushValue({ loaded, false, ValueType::I64, "" });
			return true;
		}
		case ValueType::Bool:
		{
			std::string slot = nextTemp();
			emitInstruction(slot + " = alloca i32");
			std::string fmtPtr = formatPointer("@fmt_read_i32", kFmtReadI32Len);
			emitInstruction("call i32 (i8*, ...) @scanf(i8* " + fmtPtr + ", i32* " + slot + ")");
			std::string loaded = nextTemp();
			emitInstruction(loaded + " = load i32, i32* " + slot);
			std::string cmp = nextTemp();
			emitInstruction(cmp + " = icmp ne i32 " + loaded + ", 0");
			pushValue({ cmp, false, ValueType::Bool, "" });
			return true;
		}
		case ValueType::String:
		{
			std::string buffer = nextTemp();
			emitInstruction(buffer + " = call i8* @malloc(i64 " + std::to_string(kStringReadBufferBytes) + ")");
			std::string fmtPtr = formatPointer("@fmt_read_str", kFmtReadStringLen);
			emitInstruction("call i32 (i8*, ...) @scanf(i8* " + fmtPtr + ", i8* " + buffer + ")");
			pushValue({ buffer, false, ValueType::String, "" });
			return true;
		}
		case ValueType::I32:
		default:
		{
			std::string slot = nextTemp();
			emitInstruction(slot + " = alloca i32");
			std::string fmtPtr = formatPointer("@fmt_read_i32", kFmtReadI32Len);
			emitInstruction("call i32 (i8*, ...) @scanf(i8* " + fmtPtr + ", i32* " + slot + ")");
			std::string loaded = nextTemp();
			emitInstruction(loaded + " = load i32, i32* " + slot);
			pushValue({ loaded, false, ValueType::I32, "" });
			return true;
		}
		}
	}

	return false;
}

void CodeGenerator::emitStructDefinition(const StructDeclNode& node)
{
	if (!_emittedStructs.insert(node.name()).second)
		return;

	auto infoIt = _structs.find(node.name());
	if (infoIt != _structs.end())
		emitStructDefinition(infoIt->second);
	else
	{
		_ctx.ir << "%struct." << node.name() << " = type {";
		for (size_t i = 0; i < node.fields().size(); ++i)
		{
			if (i > 0)
				_ctx.ir << ", ";
			if (node.fields()[i].type.kind == TypeDesc::Kind::Builtin)
				_ctx.ir << llvmType(node.fields()[i].type.builtin);
			else
				_ctx.ir << "%struct." << node.fields()[i].type.structName;
		}
		_ctx.ir << "}\n";
	}

	for (const auto& method : node.functions())
	{
		if (method)
			generateFunction(*method);
	}
}

void CodeGenerator::emitStructDefinition(const StructInfo& info)
{
	_ctx.ir << "%struct." << info.name << " = type {";
	for (size_t i = 0; i < info.fields.size(); ++i)
	{
		if (i > 0)
			_ctx.ir << ", ";
		if (info.fields[i].type.kind == TypeDesc::Kind::Builtin)
			_ctx.ir << llvmType(info.fields[i].type.builtin);
		else
			_ctx.ir << "%struct." << info.fields[i].type.structName;
	}
	_ctx.ir << "}\n";
}

const CodeGenerator::StringLiteralInfo& CodeGenerator::internStringLiteral(const std::string& literal)
{
	auto it = _stringLiterals.find(literal);
	if (it != _stringLiterals.end())
		return it->second;

	StringLiteralInfo info;
	info.globalName = "@.str." + std::to_string(_stringLiteralCounter++);
	info.length = literal.size() + 1;
	info.encodedValue = encodeStringLiteral(literal);
	info.emitted = false;
	auto [insertedIt, _] = _stringLiterals.emplace(literal, std::move(info));
	return insertedIt->second;
}

std::string CodeGenerator::formatPointer(const std::string& symbol, size_t length)
{
	std::string tmp = nextTemp();
	emitInstruction(tmp + " = getelementptr [" + std::to_string(length) + " x i8], [" + std::to_string(length) + " x i8]* " + symbol + ", i32 0, i32 0");
	return tmp;
}

void CodeGenerator::emitStringLiteralGlobals()
{
	for (auto& [literal, info] : _stringLiterals)
	{
		if (info.emitted)
			continue;
		_ctx.ir << info.globalName << " = private unnamed_addr constant [" << info.length << " x i8] " << info.encodedValue << "\n";
		info.emitted = true;
	}
}
