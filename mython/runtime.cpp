#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if (!object) {
        return false;
    }

    if (object.TryAs<Bool>()) {
        return object.TryAs<Bool>()->GetValue();
    }
    if (object.TryAs<Number>()) {
        return object.TryAs<Number>()->GetValue() != 0;
    }
    if (object.TryAs<String>()) {
        return !object.TryAs<String>()->GetValue().empty();
    }

    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod("__str__"s, 0)) {
        Call("__str__"s, {}, context)->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* method_ptr = class_.GetMethod(method);
    if (nullptr != method_ptr) {
        if (argument_count == method_ptr->formal_params.size()) {
            return true;
        }
    }
    return false;
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(const Class& cls)
    : class_(cls) {
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (!HasMethod(method, actual_args.size())) {
        throw std::runtime_error("The \""s + method + "\" method was not found"s);
    }
    Closure current_obj_and_his_methods;
    current_obj_and_his_methods["self"s] = ObjectHolder::Share(*this); // текущий объект
    const Method* method_ptr = class_.GetMethod(method);

    // добавление параметров вызываемого метода
    if (method_ptr->formal_params.size() > actual_args.size()) {
        throw std::runtime_error("More arguments are expected"s);
    }
    for (size_t i = 0; i < method_ptr->formal_params.size(); ++i) {
        current_obj_and_his_methods[method_ptr->formal_params[i]] = actual_args[i];
    }
    return method_ptr->body->Execute(current_obj_and_his_methods, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name)
    , methods_(forward<vector<Method>>(methods))
    , parent_(parent) {
    // возможна реализация путем сортировки methods_ и использования lower_bound
    // внутри GetName()
    for (const Method& method : methods_) {
        table_methods_[method.name] = &method;
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (table_methods_.count(name)) {
        return table_methods_.at(name);
    }
    if (parent_) {
        return parent_->GetMethod(name);
    }
    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    if (name_.empty()) {
        throw std::runtime_error("Name class is empty"s);
    }
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (!lhs && !rhs) {
        return true;
    }

    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() == rhs.TryAs<Bool>()->GetValue();
    }
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() == rhs.TryAs<Number>()->GetValue();
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() == rhs.TryAs<String>()->GetValue();
    }

    auto class_instance = lhs.TryAs<ClassInstance>();
    if (class_instance && class_instance->HasMethod("__eq__"s, 1)) {
        return class_instance->Call("__eq__"s, {rhs}, context).TryAs<Bool>()->GetValue();
    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if (lhs.TryAs<Bool>() && rhs.TryAs<Bool>()) {
        return lhs.TryAs<Bool>()->GetValue() < rhs.TryAs<Bool>()->GetValue();
    }
    if (lhs.TryAs<Number>() && rhs.TryAs<Number>()) {
        return lhs.TryAs<Number>()->GetValue() < rhs.TryAs<Number>()->GetValue();
    }
    if (lhs.TryAs<String>() && rhs.TryAs<String>()) {
        return lhs.TryAs<String>()->GetValue() < rhs.TryAs<String>()->GetValue();
    }

    auto class_instance = lhs.TryAs<ClassInstance>();
    if (class_instance && class_instance->HasMethod("__lt__"s, 1)) {
        return class_instance->Call("__lt__"s, {rhs}, context).TryAs<Bool>()->GetValue();
    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Equal(lhs, rhs, context);
    } catch (...) {
        throw;
    }
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
    } catch (...) {
        throw;
    }
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    } catch (...) {
        throw;
    }
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    try {
        return !Less(lhs, rhs, context);
    } catch (...) {
        throw;
    }
}

}  // namespace runtime
