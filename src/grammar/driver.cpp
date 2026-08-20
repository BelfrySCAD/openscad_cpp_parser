#include "driver.hpp"

namespace oscad {

namespace {
template <typename Derived>
std::vector<std::unique_ptr<Derived>> nodeListCast(NodeList list) {
    std::vector<std::unique_ptr<Derived>> result;
    result.reserve(list.size());
    for (auto& n : list) {
        result.push_back(nodeCast<Derived>(std::move(n)));
    }
    return result;
}
} // namespace

void ParserDriver::reportError(const OscadLocation& loc, const std::string& message) {
    if (hadError) {
        return; // keep only the first error
    }
    hadError = true;
    errorLine = loc.first_line;
    errorColumn = loc.first_column;
    errorOffset = loc.first_offset;
    errorReason = message;
}

std::unique_ptr<Identifier> makeIdentifier(ParserDriver& driver, const OscadLocation& loc, std::string name) {
    return std::make_unique<Identifier>(driver.toPosition(loc), std::move(name));
}

NodePtr makeStringLiteral(ParserDriver& driver, const OscadLocation& loc, std::string val) {
    return std::make_unique<StringLiteral>(driver.toPosition(loc), std::move(val));
}

NodePtr makeNumberLiteral(ParserDriver& driver, const OscadLocation& loc, double val) {
    return std::make_unique<NumberLiteral>(driver.toPosition(loc), val);
}

NodePtr makeBooleanLiteral(ParserDriver& driver, const OscadLocation& loc, bool val) {
    return std::make_unique<BooleanLiteral>(driver.toPosition(loc), val);
}

NodePtr makeUndefinedLiteral(ParserDriver& driver, const OscadLocation& loc) {
    return std::make_unique<UndefinedLiteral>(driver.toPosition(loc));
}

NodePtr makeRangeLiteral(ParserDriver& driver, const OscadLocation& loc, NodePtr start, NodePtr end, NodePtr step) {
    if (!step) {
        step = std::make_unique<NumberLiteral>(driver.toPosition(loc), 1.0);
    }
    return std::make_unique<RangeLiteral>(driver.toPosition(loc), nodeCast<Expression>(std::move(start)),
                                           nodeCast<Expression>(std::move(end)), nodeCast<Expression>(std::move(step)));
}

NodePtr makePositionalArgument(ParserDriver& driver, const OscadLocation& loc, NodePtr expr) {
    return std::make_unique<PositionalArgument>(driver.toPosition(loc), nodeCast<Expression>(std::move(expr)));
}

NodePtr makeNamedArgument(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                           NodePtr expr) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    return std::make_unique<NamedArgument>(driver.toPosition(loc), std::move(nameNode), nodeCast<Expression>(std::move(expr)));
}

NodePtr makeParameterDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc,
                                  std::string name, NodePtr defaultValue) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    std::unique_ptr<Expression> def = defaultValue ? nodeCast<Expression>(std::move(defaultValue)) : nullptr;
    return std::make_unique<ParameterDeclaration>(driver.toPosition(loc), std::move(nameNode), std::move(def));
}

NodePtr makeAssignment(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                        NodePtr expr) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    return std::make_unique<Assignment>(driver.toPosition(loc), std::move(nameNode), nodeCast<Expression>(std::move(expr)));
}

NodePtr makeLetOp(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body) {
    return std::make_unique<LetOp>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)),
                                    nodeCast<Expression>(std::move(body)));
}

NodePtr makeEchoOp(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodePtr body) {
    return std::make_unique<EchoOp>(driver.toPosition(loc), nodeListCast<Argument>(std::move(arguments)),
                                     nodeCast<Expression>(std::move(body)));
}

NodePtr makeAssertOp(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodePtr body) {
    return std::make_unique<AssertOp>(driver.toPosition(loc), nodeListCast<Argument>(std::move(arguments)),
                                       nodeCast<Expression>(std::move(body)));
}

NodePtr makeFunctionLiteral(ParserDriver& driver, const OscadLocation& loc, NodeList parameters, NodePtr body) {
    return std::make_unique<FunctionLiteral>(driver.toPosition(loc), nodeListCast<ParameterDeclaration>(std::move(parameters)),
                                              nodeCast<Expression>(std::move(body)));
}

NodePtr makeTernaryOp(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr,
                       NodePtr falseExpr) {
    return std::make_unique<TernaryOp>(driver.toPosition(loc), nodeCast<Expression>(std::move(condition)),
                                        nodeCast<Expression>(std::move(trueExpr)), nodeCast<Expression>(std::move(falseExpr)));
}

NodePtr makePrimaryCall(ParserDriver& driver, const OscadLocation& loc, NodePtr left, NodeList arguments) {
    return std::make_unique<PrimaryCall>(driver.toPosition(loc), nodeCast<Expression>(std::move(left)),
                                          nodeListCast<Argument>(std::move(arguments)));
}

NodePtr makePrimaryIndex(ParserDriver& driver, const OscadLocation& loc, NodePtr left, NodePtr index) {
    return std::make_unique<PrimaryIndex>(driver.toPosition(loc), nodeCast<Expression>(std::move(left)),
                                           nodeCast<Expression>(std::move(index)));
}

NodePtr makePrimaryMember(ParserDriver& driver, const OscadLocation& loc, NodePtr left, const OscadLocation& memberLoc,
                           std::string member) {
    auto memberNode = makeIdentifier(driver, memberLoc, std::move(member));
    return std::make_unique<PrimaryMember>(driver.toPosition(loc), nodeCast<Expression>(std::move(left)), std::move(memberNode));
}

NodePtr makeListCompLet(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body) {
    return std::make_unique<ListCompLet>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)),
                                          std::move(body));
}

NodePtr makeListCompEach(ParserDriver& driver, const OscadLocation& loc, NodePtr body) {
    return std::make_unique<ListCompEach>(driver.toPosition(loc), std::move(body));
}

NodePtr makeListCompFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodePtr body) {
    return std::make_unique<ListCompFor>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)),
                                          std::move(body));
}

NodePtr makeListCompCFor(ParserDriver& driver, const OscadLocation& loc, NodeList inits, NodePtr condition, NodeList incrs,
                          NodePtr body) {
    return std::make_unique<ListCompCFor>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(inits)),
                                           nodeCast<Expression>(std::move(condition)), nodeListCast<Assignment>(std::move(incrs)),
                                           std::move(body));
}

NodePtr makeListCompIf(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr) {
    return std::make_unique<ListCompIf>(driver.toPosition(loc), nodeCast<Expression>(std::move(condition)), std::move(trueExpr));
}

NodePtr makeListCompIfElse(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodePtr trueExpr,
                            NodePtr falseExpr) {
    return std::make_unique<ListCompIfElse>(driver.toPosition(loc), nodeCast<Expression>(std::move(condition)),
                                             std::move(trueExpr), std::move(falseExpr));
}

NodePtr makeListComprehension(ParserDriver& driver, const OscadLocation& loc, NodeList elements) {
    return std::make_unique<ListComprehension>(driver.toPosition(loc), std::move(elements));
}

NodePtr makeRenderExpression(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodeList children) {
    return std::make_unique<RenderExpression>(driver.toPosition(loc), nodeListCast<Argument>(std::move(arguments)),
                                              std::move(children));
}

NodePtr makeModularCall(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                         NodeList arguments, NodeList children) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    return std::make_unique<ModularCall>(driver.toPosition(loc), std::move(nameNode), nodeListCast<Argument>(std::move(arguments)),
                                         std::move(children));
}

NodePtr makeModularFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList body) {
    return std::make_unique<ModularFor>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)), std::move(body));
}

NodePtr makeModularIntersectionFor(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList body) {
    return std::make_unique<ModularIntersectionFor>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)),
                                                     std::move(body));
}

NodePtr makeModularLet(ParserDriver& driver, const OscadLocation& loc, NodeList assignments, NodeList children) {
    return std::make_unique<ModularLet>(driver.toPosition(loc), nodeListCast<Assignment>(std::move(assignments)),
                                        std::move(children));
}

NodePtr makeModularEcho(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodeList children) {
    return std::make_unique<ModularEcho>(driver.toPosition(loc), nodeListCast<Argument>(std::move(arguments)),
                                         std::move(children));
}

NodePtr makeModularAssert(ParserDriver& driver, const OscadLocation& loc, NodeList arguments, NodeList children) {
    return std::make_unique<ModularAssert>(driver.toPosition(loc), nodeListCast<Argument>(std::move(arguments)),
                                           std::move(children));
}

NodePtr makeModularIf(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodeList trueBranch) {
    return std::make_unique<ModularIf>(driver.toPosition(loc), nodeCast<Expression>(std::move(condition)), std::move(trueBranch));
}

NodePtr makeModularIfElse(ParserDriver& driver, const OscadLocation& loc, NodePtr condition, NodeList trueBranch,
                           NodeList falseBranch) {
    return std::make_unique<ModularIfElse>(driver.toPosition(loc), nodeCast<Expression>(std::move(condition)),
                                           std::move(trueBranch), std::move(falseBranch));
}

NodePtr makeModuleDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                               NodeList parameters, NodeList children) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    return std::make_unique<ModuleDeclaration>(driver.toPosition(loc), std::move(nameNode),
                                               nodeListCast<ParameterDeclaration>(std::move(parameters)), std::move(children));
}

NodePtr makeFunctionDeclaration(ParserDriver& driver, const OscadLocation& loc, const OscadLocation& nameLoc, std::string name,
                                 NodeList parameters, NodePtr expr) {
    auto nameNode = makeIdentifier(driver, nameLoc, std::move(name));
    return std::make_unique<FunctionDeclaration>(driver.toPosition(loc), std::move(nameNode),
                                                 nodeListCast<ParameterDeclaration>(std::move(parameters)),
                                                 nodeCast<Expression>(std::move(expr)));
}

NodePtr makeUseStatement(ParserDriver& driver, const OscadLocation& loc, std::string path) {
    auto pathNode = std::make_unique<StringLiteral>(driver.toPosition(loc), std::move(path));
    return std::make_unique<UseStatement>(driver.toPosition(loc), std::move(pathNode));
}

NodePtr makeIncludeStatement(ParserDriver& driver, const OscadLocation& loc, std::string path) {
    auto pathNode = std::make_unique<StringLiteral>(driver.toPosition(loc), std::move(path));
    return std::make_unique<IncludeStatement>(driver.toPosition(loc), std::move(pathNode));
}

} // namespace oscad
