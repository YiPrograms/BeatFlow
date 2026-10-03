#pragma once

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "custom-types/shared/macros.hpp"

#ifndef DECLARE_OVERRIDE_METHOD_MATCH
#define DECLARE_OVERRIDE_METHOD_MATCH(returnType, methodName, methodPointer, ...)                            \
    DECLARE_OVERRIDE_METHOD(returnType, methodName,                                                          \
                            il2cpp_utils::il2cpp_type_check::MetadataGetter<methodPointer>::get(),           \
                            __VA_ARGS__)
#endif
