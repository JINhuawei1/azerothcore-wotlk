/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Field.h"
#include "Errors.h"
#include "Log.h"
#include "MySQLHacks.h"
#include "StringConvert.h"

Field::Field()
{
    data.value = nullptr;
    data.length = 0;
    data.raw = false;
    meta = nullptr;
}

namespace
{
    template<typename T>
    constexpr T GetDefaultValue()
    {
        if constexpr (std::is_same_v<T, bool>)
            return false;
        else if constexpr (std::is_integral_v<T>)
            return 0;
        else if constexpr (std::is_floating_point_v<T>)
            return 1.0f;
        else if constexpr (std::is_same_v<T, std::vector<uint8>> || std::is_same_v<std::string_view, T>)
            return {};
        else
            return "";
    }

    template<typename T>
    inline bool IsCorrectFieldType(DatabaseFieldTypes type)
    {
        // Int8
        if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int8> || std::is_same_v<T, uint8>)
        {
            if (type == DatabaseFieldTypes::Int8)
                return true;
        }

        // In16
        if constexpr (std::is_same_v<T, uint16> || std::is_same_v<T, int16>)
        {
            if (type == DatabaseFieldTypes::Int16)
                return true;
        }

        // Int32
        if constexpr (std::is_same_v<T, uint32> || std::is_same_v<T, int32>)
        {
            if (type == DatabaseFieldTypes::Int32)
                return true;
        }

        // Int64
        if constexpr (std::is_same_v<T, uint64> || std::is_same_v<T, int64>)
        {
            if (type == DatabaseFieldTypes::Int64)
                return true;
        }

        // float
        if constexpr (std::is_same_v<T, float>)
        {
            if (type == DatabaseFieldTypes::Float)
                return true;
        }

        // dobule
        if constexpr (std::is_same_v<T, double>)
        {
            if (type == DatabaseFieldTypes::Double || type == DatabaseFieldTypes::Decimal)
                return true;
        }

        // Binary
        if constexpr (std::is_same_v<T, Binary>)
        {
            if (type == DatabaseFieldTypes::Binary)
                return true;
        }

        return false;
    }

    inline Optional<std::string_view> GetCleanAliasName(std::string_view alias)
    {
        if (alias.empty())
            return {};

        auto pos = alias.find_first_of('(');
        if (pos == std::string_view::npos)
            return {};

        alias.remove_suffix(alias.length() - pos);

        return { alias };
    }

    template<typename T>
    inline bool IsCorrectAlias(DatabaseFieldTypes type, std::string_view alias)
    {
        if constexpr (std::is_same_v<T, double>)
        {
            if ((StringEqualI(alias, "sum") || StringEqualI(alias, "avg")) && type == DatabaseFieldTypes::Decimal)
                return true;

            return false;
        }

        if constexpr (std::is_same_v<T, uint64>)
        {
            if (StringEqualI(alias, "count") && type == DatabaseFieldTypes::Int64)
                return true;

            return false;
        }

        if ((StringEqualI(alias, "min") || StringEqualI(alias, "max")) && IsCorrectFieldType<T>(type))
        {
            return true;
        }

        return false;
    }

    Optional<int256> GetRawInt256(char const* value, uint32 length, DatabaseFieldTypes type)
    {
        switch (type)
        {
            case DatabaseFieldTypes::Int8:
                ASSERT(length == sizeof(int8), "Expected {}-byte int8 raw field, got {} bytes instead", sizeof(int8), length);
                return static_cast<int256>(*reinterpret_cast<int8 const*>(value));
            case DatabaseFieldTypes::Int16:
                ASSERT(length == sizeof(int16), "Expected {}-byte int16 raw field, got {} bytes instead", sizeof(int16), length);
                return static_cast<int256>(*reinterpret_cast<int16 const*>(value));
            case DatabaseFieldTypes::Int32:
                ASSERT(length == sizeof(int32), "Expected {}-byte int32 raw field, got {} bytes instead", sizeof(int32), length);
                return static_cast<int256>(*reinterpret_cast<int32 const*>(value));
            case DatabaseFieldTypes::Int64:
                ASSERT(length == sizeof(int64), "Expected {}-byte int64 raw field, got {} bytes instead", sizeof(int64), length);
                return static_cast<int256>(*reinterpret_cast<int64 const*>(value));
            default:
                return {};
        }
    }

    Optional<uint256> GetRawUInt256(char const* value, uint32 length, DatabaseFieldTypes type)
    {
        switch (type)
        {
            case DatabaseFieldTypes::Int8:
                ASSERT(length == sizeof(uint8), "Expected {}-byte uint8 raw field, got {} bytes instead", sizeof(uint8), length);
                return static_cast<uint256>(*reinterpret_cast<uint8 const*>(value));
            case DatabaseFieldTypes::Int16:
                ASSERT(length == sizeof(uint16), "Expected {}-byte uint16 raw field, got {} bytes instead", sizeof(uint16), length);
                return static_cast<uint256>(*reinterpret_cast<uint16 const*>(value));
            case DatabaseFieldTypes::Int32:
                ASSERT(length == sizeof(uint32), "Expected {}-byte uint32 raw field, got {} bytes instead", sizeof(uint32), length);
                return static_cast<uint256>(*reinterpret_cast<uint32 const*>(value));
            case DatabaseFieldTypes::Int64:
                ASSERT(length == sizeof(uint64), "Expected {}-byte uint64 raw field, got {} bytes instead", sizeof(uint64), length);
                return static_cast<uint256>(*reinterpret_cast<uint64 const*>(value));
            default:
                return {};
        }
    }
}

void Field::GetBinarySizeChecked(uint8* buf, std::size_t length) const
{
    ASSERT(data.value && (data.length == length), "Expected {}-byte binary blob, got {}data ({} bytes) instead", length, data.value ? "" : "no ", data.length);
    memcpy(buf, data.value, length);
}

void Field::SetByteValue(char const* newValue, uint32 length)
{
    // This value stores raw bytes that have to be explicitly cast later
    data.value = newValue;
    data.length = length;
    data.raw = true;
}

void Field::SetStructuredValue(char const* newValue, uint32 length)
{
    // This value stores somewhat structured data that needs function style casting
    data.value = newValue;
    data.length = length;
    data.raw = false;
}

bool Field::IsType(DatabaseFieldTypes type) const
{
    return meta->Type == type;
}

bool Field::IsNumeric() const
{
    return (meta->Type == DatabaseFieldTypes::Int8 ||
        meta->Type == DatabaseFieldTypes::Int16 ||
        meta->Type == DatabaseFieldTypes::Int32 ||
        meta->Type == DatabaseFieldTypes::Int64 ||
        meta->Type == DatabaseFieldTypes::Float ||
        meta->Type == DatabaseFieldTypes::Double);
}

void Field::LogWrongType(std::string_view getter, std::string_view typeName) const
{
    LOG_WARN("sql.sql", "Warning: {}<{}> on {} field {}.{} ({}.{}) at index {}.",
        getter, typeName, meta->TypeName, meta->TableAlias, meta->Alias, meta->TableName, meta->Name, meta->Index);
}

void Field::SetMetadata(QueryResultFieldMetadata const* fieldMeta)
{
    meta = fieldMeta;
}

template<typename T>
T Field::GetData() const
{
    static_assert(std::is_arithmetic_v<T>, "Unsurropt type for Field::GetData()");

    if (!data.value)
        return GetDefaultValue<T>();

#ifdef ACORE_STRICT_DATABASE_TYPE_CHECKS
    if (!IsCorrectFieldType<T>(meta->Type))
    {
        LogWrongType(__FUNCTION__, typeid(T).name());
        //return GetDefaultValue<T>();
    }
#endif

    Optional<T> result = {};

    if (data.raw)
        result = *reinterpret_cast<T const*>(data.value);
    else
        result = Acore::StringTo<T>(std::string_view(data.value, data.length));

    // Correct double fields... this undefined behavior :/
    if constexpr (std::is_same_v<T, double>)
    {
        if (data.raw && !IsType(DatabaseFieldTypes::Decimal))
            result = *reinterpret_cast<double const*>(data.value);
        else
            result = Acore::StringTo<float>(std::string_view(data.value, data.length));
    }

    // Check -1 for *_dbc db tables
    if constexpr (std::is_same_v<T, uint32>)
    {
        std::string_view tableName{ meta->TableName };

        if (!tableName.empty() && tableName.size() > 4)
        {
            auto signedResult = Acore::StringTo<int32>(std::string_view(data.value, data.length));

            if (signedResult && !result && tableName.substr(tableName.length() - 4) == "_dbc")
            {
                LOG_DEBUG("sql.sql", "> Found incorrect value '{}' for type '{}' in _dbc table.", data.value, typeid(T).name());
                LOG_DEBUG("sql.sql", "> Table name '{}'. Field name '{}'. Try return int32 value", meta->TableName, meta->Name);
                return GetData<int32>();
            }
        }
    }

    if (auto alias = GetCleanAliasName(meta->Alias))
    {
        if ((StringEqualI(*alias, "min") || StringEqualI(*alias, "max")) && !IsCorrectAlias<T>(meta->Type, *alias))
        {
            LogWrongType(__FUNCTION__, typeid(T).name());
        }

        if ((StringEqualI(*alias, "sum") || StringEqualI(*alias, "avg")) && !IsCorrectAlias<T>(meta->Type, *alias))
        {
            LogWrongType(__FUNCTION__, typeid(T).name());
            LOG_WARN("sql.sql", "> Please use GetData<double>()");
            return GetData<double>();
        }

        if (StringEqualI(*alias, "count") && !IsCorrectAlias<T>(meta->Type, *alias))
        {
            LogWrongType(__FUNCTION__, typeid(T).name());
            LOG_WARN("sql.sql", "> Please use GetData<uint64>()");
            return GetData<uint64>();
        }
    }

    if (!result)
    {
        LOG_FATAL("sql.sql", "> Incorrect value '{}' for type '{}'. Value is raw ? '{}'", data.value, typeid(T).name(), data.raw);
        LOG_FATAL("sql.sql", "> Table name '{}'. Field name '{}'", meta->TableName, meta->Name);
        //ABORT();
        return GetDefaultValue<T>();
    }

    return *result;
}

template bool Field::GetData() const;
template uint8 Field::GetData() const;
template uint16 Field::GetData() const;
template uint32 Field::GetData() const;
template uint64 Field::GetData() const;
template int8 Field::GetData() const;
template int16 Field::GetData() const;
template int32 Field::GetData() const;
template int64 Field::GetData() const;
template float Field::GetData() const;
template double Field::GetData() const;

int256 Field::GetInt256() const
{
    if (!data.value)
        return 0;

    if (data.raw)
    {
        Optional<int256> rawResult = GetRawInt256(data.value, data.length, meta->Type);
        if (rawResult)
            return *rawResult;
    }

    Optional<int256> result = Acore::StringTo<int256>(std::string_view(data.value, data.length));
    if (!result)
    {
        LOG_FATAL("sql.sql", "> Incorrect value '{}' for type 'int256'. Value is raw ? '{}'", std::string_view(data.value, data.length), data.raw);
        LOG_FATAL("sql.sql", "> Table name '{}'. Field name '{}'", meta->TableName, meta->Name);
        return 0;
    }

    return *result;
}

uint256 Field::GetUInt256() const
{
    if (!data.value)
        return 0;

    if (data.raw)
    {
        Optional<uint256> rawResult = GetRawUInt256(data.value, data.length, meta->Type);
        if (rawResult)
            return *rawResult;
    }

    Optional<uint256> result = Acore::StringTo<uint256>(std::string_view(data.value, data.length));
    if (!result)
    {
        LOG_FATAL("sql.sql", "> Incorrect value '{}' for type 'uint256'. Value is raw ? '{}'", std::string_view(data.value, data.length), data.raw);
        LOG_FATAL("sql.sql", "> Table name '{}'. Field name '{}'", meta->TableName, meta->Name);
        return 0;
    }

    return *result;
}

std::string Field::GetDataString() const
{
    if (!data.value)
        return "";

#ifdef ACORE_STRICT_DATABASE_TYPE_CHECKS
    if (IsNumeric() && data.raw)
    {
        LogWrongType(__FUNCTION__, "std::string");
        return "";
    }
#endif

    return { data.value, data.length };
}

std::string_view Field::GetDataStringView() const
{
    if (!data.value)
        return {};

#ifdef ACORE_STRICT_DATABASE_TYPE_CHECKS
    if (IsNumeric() && data.raw)
    {
        LogWrongType(__FUNCTION__, "std::string_view");
        return {};
    }
#endif

    return { data.value, data.length };
}

Binary Field::GetDataBinary() const
{
    Binary result = {};
    if (!data.value || !data.length)
        return result;

#ifdef ACORE_STRICT_DATABASE_TYPE_CHECKS
    if (!IsCorrectFieldType<Binary>(meta->Type))
    {
        LogWrongType(__FUNCTION__, "Binary");
        return {};
    }
#endif

    result.resize(data.length);
    memcpy(result.data(), data.value, data.length);
    return result;
}
