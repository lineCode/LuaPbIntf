#include "GetMessageField.h"

#include "MessageSptr.h"  // for MessageSptr

#include <LuaIntf/LuaIntf.h>
#include <google/protobuf/descriptor.h>  // for Descriptor
#include <google/protobuf/message.h>  // for Message
#include "google/protobuf/descriptor.pb.h"  // for map_entry()

using namespace LuaIntf;
using namespace google::protobuf;
using namespace std;

namespace {

MessageSptr MakeSharedMessage(const Message& msg)
{
    MessageSptr pMsg(msg.New());
    pMsg->CopyFrom(msg);
    return pMsg;
}

inline LuaRef MessageResult(lua_State* L, const Message& msg)
{
    return LuaRefValue(L, MakeSharedMessage(msg));
}

// Set lua table element value.
// Use SetTblMessageElement() if element is a Message.
template <typename T>
void SetTblValueElement(LuaRef& rTbl, int index, const T& value)
{
    rTbl.set(index, value);
}

// Set lua table element to Message.
// Support map entry message.
void SetTblMessageElement(LuaRef& rTbl, int index, const Message& msg)
{
    const Descriptor* pDesc = msg.GetDescriptor();
    assert(pDesc);
    bool isMap = pDesc->options().map_entry();
    if (!isMap)
    {
        rTbl.set(index, MakeSharedMessage(msg));
        return;
    }

    // XXX
}

// XXX Use RepeatedFieldRef

// Get repeated field element and insert it to lua table.
// Map field is supported.
// Returns empty if success.
string GetRepeatedFieldElement(const Message& msg,
    const FieldDescriptor& field, int index, LuaRef& rTbl)
{
    assert(field.is_repeated());
    assert(index >= 0);
    const Reflection* pRefl = msg.GetReflection();
    assert(pRefl);
    assert(index < pRefl->FieldSize(msg, &field));

    using Fd = FieldDescriptor;
    Fd::CppType eCppType = field.cpp_type();
    switch (eCppType)
    {
    case Fd::CPPTYPE_INT32:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedInt32(msg, &field, index));
        return "";
    case Fd::CPPTYPE_INT64:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedInt64(msg, &field, index));
        return "";
    case Fd::CPPTYPE_UINT32:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedUInt32(msg, &field, index));
        return "";
    case Fd::CPPTYPE_UINT64:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedUInt64(msg, &field, index));
        return "";
    case Fd::CPPTYPE_DOUBLE:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedDouble(msg, &field, index));
        return "";
    case Fd::CPPTYPE_FLOAT:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedFloat(msg, &field, index));
        return "";
    case Fd::CPPTYPE_BOOL:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedBool(msg, &field, index));
        return "";
    case Fd::CPPTYPE_ENUM:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedEnumValue(msg, &field, index));
        return "";
    case Fd::CPPTYPE_STRING:
        SetTblValueElement(rTbl, index,
            pRefl->GetRepeatedString(msg, &field, index));
        return "";
    case Fd::CPPTYPE_MESSAGE:
        // Support map entry element.
        SetTblMessageElement(rTbl, index,
            pRefl->GetRepeatedMessage(msg, &field, index));
        return "";
    default:
        break;
    }
    // Unknown repeated field type CPPTYPE_UNKNOWN of Message.Field
    return string("Unknown repeated field type ") +
        field.CppTypeName(eCppType) + " of " + field.full_name();
}

// returns (TableRef, "") or (nil, error_string)
// Map is supported.
LuaRef GetRepeatedField(lua_State& luaState,
    const Message& msg, const FieldDescriptor& field)
{
    assert(field.is_repeated());
    const Reflection* pRefl = msg.GetReflection();
    assert(pRefl);

    LuaRef tbl = LuaRef::createTable(&luaState);
    int nFldSize = pRefl->FieldSize(msg, &field);
    for (int index = 0; index < nFldSize; ++index)
    {
        string sError = GetRepeatedFieldElement(msg, field, index, tbl);
        if (!sError.empty())
            throw LuaException(sError);
    }
    return tbl;
}

}  // namespace

LuaRef GetMessageField(lua_State& rLuaState,
    const Message& msg, const FieldDescriptor& field)
{
    const Descriptor* pDesc = msg.GetDescriptor();
    assert(pDesc);
    const Reflection* pRefl = msg.GetReflection();
    if (!pRefl)
        throw LuaException("Message " + msg.GetTypeName() + " has no reflection.");

    if (field.is_repeated())
    {
        // returns (TableRef, "") or (nil, error_string)
        return GetRepeatedField(rLuaState, msg, field);
    }

    using Fd = FieldDescriptor;
    lua_State* L = &rLuaState;
    Fd::CppType eCppType = field.cpp_type();
    switch (eCppType)
    {
    // Scalar field always has a default value.
    case Fd::CPPTYPE_INT32:
        return LuaRefValue(L, pRefl->GetInt32(msg, &field));
    case Fd::CPPTYPE_INT64:
        return LuaRefValue(L, pRefl->GetInt64(msg, &field));
    case Fd::CPPTYPE_UINT32:
        return LuaRefValue(L, pRefl->GetUInt32(msg, &field));
    case Fd::CPPTYPE_UINT64:
        return LuaRefValue(L, pRefl->GetUInt64(msg, &field));
    case Fd::CPPTYPE_DOUBLE:
        return LuaRefValue(L, pRefl->GetDouble(msg, &field));
    case Fd::CPPTYPE_FLOAT:
        return LuaRefValue(L, pRefl->GetFloat(msg, &field));
    case Fd::CPPTYPE_BOOL:
        return LuaRefValue(L, pRefl->GetBool(msg, &field));
    case Fd::CPPTYPE_ENUM:
        return LuaRefValue(L, pRefl->GetEnumValue(msg, &field));
    case Fd::CPPTYPE_STRING:
        return LuaRefValue(L, pRefl->GetString(msg, &field));
    case Fd::CPPTYPE_MESSAGE:
        // For message field, the default value is null.
        if (pRefl->HasField(msg, &field))
            return MessageResult(L, pRefl->GetMessage(msg, &field));
        return LuaRefValue(L, nullptr);
    default:
        break;
    }
    // Unknown field type CPPTYPE_UNKNOWN of Message.Field
    throw LuaException(string("Unknown field type ") +
        field.CppTypeName(eCppType) + " of " + field.full_name());
}
