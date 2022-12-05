#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

// =====================================================================================

VariableValue::VariableValue(const std::string& var_name)
    : dotted_ids_({var_name}) {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : dotted_ids_(move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    size_t ids_count = dotted_ids_.size();
    Closure new_closure = closure;
    for (size_t i = 0; i < ids_count; ++i) {
        if (new_closure.count(dotted_ids_[i])) {
            if (i == ids_count - 1) {
                return new_closure.at(dotted_ids_[i]);
            }
            // рекуррентно углубляемся в поля
            if (runtime::ClassInstance* base_ci = new_closure.at(dotted_ids_[i]).TryAs<runtime::ClassInstance>()) {
                new_closure = base_ci->Fields();
            }
        }
    }
    throw std::runtime_error("Unexpected error of the \"VariableValue::Execute\" method"s);
}

// =====================================================================================

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : variable_name_(move(var))
    , r_value_(move(rv)) {
}

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[variable_name_] = r_value_->Execute(closure, context);
    return closure.at(variable_name_);
}

// =====================================================================================

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
    : object_(move(object))
    , field_name_(move(field_name))
    , r_value_(move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = object_.Execute(closure, context);
    if (!obj.TryAs<runtime::ClassInstance>()) {
        throw runtime_error("FieldAssignment::Execute. The object is not a custom type"s);
    }
    // устанавливаемое поле
    ObjectHolder& field = obj.TryAs<runtime::ClassInstance>()->Fields()[field_name_];
    field = r_value_->Execute(closure, context);
    closure[field_name_] = field;
    return field;
}

// =====================================================================================

runtime::ObjectHolder None::Execute([[maybe_unused]] runtime::Closure& closure,
                                    [[maybe_unused]] runtime::Context& context) {
    return {};
}

// =====================================================================================

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) :
    args_(move(args)) {
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return make_unique<Print>(make_unique<VariableValue>(name));
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    ostringstream os;
    for (size_t i = 0; i < args_.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        ObjectHolder oh = args_[i].get()->Execute(closure, context);
        if (runtime::Object* obj = oh.Get()) {
            obj->Print(os, context);
        } else {
            os << "None"s;
        }
    }
    context.GetOutputStream() << os.str() << endl;
    runtime::String str_obj(os.str());
    return ObjectHolder::Own(move(str_obj));
}

// =====================================================================================

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
    : object_(move(object))
    , method_(move(method))
    , args_(move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    runtime::ClassInstance* class_instance = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();

    if (!class_instance) {
        throw std::runtime_error("MethodCall::Execute. The object is not a custom type"s);
    }
    if (!class_instance->HasMethod(method_, args_.size())) {
        throw std::runtime_error("MethodCall::Execute. The class does not have a \"" + method_ +
                                 "\" method with " + to_string(args_.size()) + " arguments"s);
    }

    vector<runtime::ObjectHolder> actual_args;
    actual_args.reserve(args_.size());
    for (const unique_ptr<Statement>& argument : args_) {
        actual_args.push_back(argument->Execute(closure, context));
    }

    return class_instance->Call(method_, actual_args, context);
}

// =====================================================================================

ast::NewInstance::NewInstance(const runtime::Class& class_,
                         std::vector<std::unique_ptr<Statement>> args) :
    class_instance_(class_),
    args_(move(args)) {
}

NewInstance::NewInstance(const runtime::Class& class_) :
    class_instance_(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (class_instance_.HasMethod(INIT_METHOD, args_.size())) {
        vector<ObjectHolder> actual_args;
        actual_args.reserve(args_.size());

        for (const unique_ptr<Statement>& arg : args_) {
            actual_args.push_back(arg->Execute(closure, context));
        }
        class_instance_.Call(INIT_METHOD, actual_args, context);
    }
    return ObjectHolder::Share(class_instance_);
}

// =====================================================================================

UnaryOperation::UnaryOperation(std::unique_ptr<Statement> argument) :
    argument_(move(argument)) {
}

// =====================================================================================

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    ostringstream out;
    runtime::Object* obj_ptr = argument_->Execute(closure, context).Get();
    if (!obj_ptr) {
        out << "None"s;
    } else {
        obj_ptr->Print(out, context);
    }
    // метод не должен изменять context, только возвращать строку
    return ObjectHolder::Own(runtime::String(out.str()));
}

// =====================================================================================

BinaryOperation::BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs)
    : lhs_(move(lhs))
    , rhs_(move(rhs)) {
}

// =====================================================================================

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() + rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    } else if (auto* lhs_s = lhs_oh.TryAs<runtime::String>()) {
        if (auto* rhs_s = rhs_oh.TryAs<runtime::String>()) {
            string str = lhs_s->GetValue() + rhs_s->GetValue();
            runtime::String result(str);
            return ObjectHolder::Own(move(result));
        }
    } else if (auto* lhs_ci = lhs_oh.TryAs<runtime::ClassInstance>()) {
        if (lhs_ci->HasMethod(ADD_METHOD, 1)) {
            return lhs_ci->Call(ADD_METHOD, { rhs_oh }, context);
        }
    }
    throw runtime_error("Add::Execute is failed"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() - rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Sub::Execute is failed"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            int val = lhs_n->GetValue() * rhs_n->GetValue();
            runtime::Number result(val);
            return ObjectHolder::Own(move(result));
        }
    }
    throw runtime_error("Mult::Execute is failed"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs_oh = lhs_.get()->Execute(closure, context);
    ObjectHolder rhs_oh = rhs_.get()->Execute(closure, context);
    if (auto* lhs_n = lhs_oh.TryAs<runtime::Number>()) {
        if (auto* rhs_n = rhs_oh.TryAs<runtime::Number>()) {
            if (int rhs_val = rhs_n->GetValue()) {
                int val = lhs_n->GetValue() / rhs_val;
                runtime::Number result(val);
                return ObjectHolder::Own(move(result));
            }
        }
    }
    throw runtime_error("Div::Execute is failed"s);
}

// =====================================================================================

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    runtime::Bool* lhs_ptr = lhs_->Execute(closure, context).TryAs<runtime::Bool>();
    if (lhs_ptr && lhs_ptr->GetValue()) {
        return ObjectHolder::Own(runtime::Bool(true));
    }

    runtime::Bool* rhs_ptr = rhs_->Execute(closure, context).TryAs<runtime::Bool>();
    if (rhs_ptr && rhs_ptr->GetValue()) {
        return ObjectHolder::Own(runtime::Bool(true));
    }

    return ObjectHolder::Own(runtime::Bool(false));
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    runtime::Bool* lhs_ptr = lhs_->Execute(closure, context).TryAs<runtime::Bool>();
    if (!lhs_ptr || !lhs_ptr->GetValue()) {
        return ObjectHolder::Own(runtime::Bool(false));
    }

    runtime::Bool* rhs_ptr = rhs_->Execute(closure, context).TryAs<runtime::Bool>();
    if (!rhs_ptr || !rhs_ptr->GetValue()) {
        return ObjectHolder::Own(runtime::Bool(false));
    }

    return ObjectHolder::Own(runtime::Bool(true));
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    runtime::Bool* arg_ptr = argument_->Execute(closure, context).TryAs<runtime::Bool>();
    if (!arg_ptr) {
        throw std::runtime_error("Not::Execute. The argument cannot be cast to the \"runtime::Bool\" type"s);
    }

    return arg_ptr->GetValue() ? ObjectHolder::Own(runtime::Bool(false)) : ObjectHolder::Own(runtime::Bool(true));
}

// =====================================================================================

void Compound::AddStatement(std::unique_ptr<Statement> stmt) {
    statements_.push_back(move(stmt));
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const unique_ptr<Statement>& statement : statements_) {
        statement->Execute(closure, context);
    }
    return ObjectHolder::None();
}

// =====================================================================================

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        return body_->Execute(closure, context);
    } catch (ObjectHolder result) {
        return result;
    }
}

// =====================================================================================

Return::Return(std::unique_ptr<Statement> statement) :
    statement_(move(statement)) {
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_->Execute(closure, context);
}

// =====================================================================================

ClassDefinition::ClassDefinition(ObjectHolder cls)
    : class_(move(cls)) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    closure[class_.TryAs<runtime::Class>()->GetName()] = class_;
    return class_;
}

// =====================================================================================

IfElse::IfElse(std::unique_ptr<Statement> condition,
               std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body) :
    condition_(move(condition)),
    if_body_(move(if_body)),
    else_body_(move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    ObjectHolder condition = condition_.get()->Execute(closure, context);
    if (auto* b = condition.TryAs<runtime::Bool>()) {
        if (b->GetValue()) {
            return if_body_->Execute(closure, context);
        } else if (else_body_.get()) {
            return else_body_->Execute(closure, context);
        } else {
            return ObjectHolder::None();
        }
    }
    throw runtime_error(string(__func__) + " is failed");
}

// =====================================================================================

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(cmp) {
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context);
    ObjectHolder rhs = rhs_->Execute(closure, context);
    bool result = cmp_(lhs, rhs, context);
    return ObjectHolder::Own(runtime::Bool(result));
}

}  // namespace ast
