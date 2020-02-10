/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file SimpleTablePrecompiled.h
 *  @author xingqiangbai
 *  @date 20200206
 */
#include "SimpleTablePrecompiled.h"
#include "Common.h"
#include "ConditionPrecompiled.h"
#include "EntriesPrecompiled.h"
#include "EntryPrecompiled.h"
#include "libstorage/StorageException.h"
#include "libstorage/Table.h"
#include <libblockverifier/ExecutiveContext.h>
#include <libethcore/ABI.h>

using namespace dev;
using namespace dev::storage;
using namespace dev::precompiled;
using namespace dev::blockverifier;

const char* const SIMPLETABLE_METHOD_GET = "get(string)";
const char* const SIMPLETABLE_METHOD_SET = "set(string,address)";
const char* const SIMPLETABLE_METHOD_NEWENT = "newEntry()";


SimpleTablePrecompiled::SimpleTablePrecompiled()
{
    name2Selector[SIMPLETABLE_METHOD_GET] = getFuncSelector(SIMPLETABLE_METHOD_GET);
    name2Selector[SIMPLETABLE_METHOD_SET] = getFuncSelector(SIMPLETABLE_METHOD_SET);
    name2Selector[SIMPLETABLE_METHOD_NEWENT] = getFuncSelector(SIMPLETABLE_METHOD_NEWENT);
}

std::string SimpleTablePrecompiled::toString()
{
    return "SimpleTable";
}

bytes SimpleTablePrecompiled::call(
    ExecutiveContext::Ptr context, bytesConstRef param, Address const& origin)
{
    PRECOMPILED_LOG(TRACE) << LOG_BADGE("SimpleTablePrecompiled") << LOG_DESC("call")
                           << LOG_KV("param", toHex(param));

    uint32_t func = getParamFunc(param);
    bytesConstRef data = getParamData(param);

    dev::eth::ContractABI abi;

    bytes out;

    if (func == name2Selector[SIMPLETABLE_METHOD_GET])
    {  // get(string)
        std::string key;
        abi.abiOut(data, key);

        auto entries = m_table->select(key, m_table->newCondition());
        if (entries->size() == 0)
        {
            out = abi.abiIn("", false, Address());
        }
        else
        {
            auto entryPrecompiled = std::make_shared<EntryPrecompiled>();
            // CachedStorage return entry use copy from
            entryPrecompiled->setEntry(
                std::const_pointer_cast<dev::storage::Entries>(entries)->get(0));
            auto newAddress = context->registerPrecompiled(entryPrecompiled);
            out = abi.abiIn("", true, newAddress);
        }
    }
    else if (func == name2Selector[SIMPLETABLE_METHOD_SET])
    {  // set(string,address)
        std::string key;
        Address entryAddress;
        abi.abiOut(data, key, entryAddress);

        EntryPrecompiled::Ptr entryPrecompiled =
            std::dynamic_pointer_cast<EntryPrecompiled>(context->getPrecompiled(entryAddress));
        auto entry = entryPrecompiled->getEntry();
        checkLengthValidate(
            key, USER_TABLE_KEY_VALUE_MAX_LENGTH, CODE_TABLE_KEYVALUE_LENGTH_OVERFLOW, false);

        auto it = entry->begin();
        for (; it != entry->end(); ++it)
        {
            checkLengthValidate(it->second, USER_TABLE_FIELD_VALUE_MAX_LENGTH,
                CODE_TABLE_KEYVALUE_LENGTH_OVERFLOW, false);
        }
        auto entries = m_table->select(key, m_table->newCondition());
        int count = 0;
        if (entries->size() == 0)
        {
            count = m_table->insert(key, entry, std::make_shared<AccessOptions>(origin));
        }
        else
        {
            count = m_table->update(
                key, entry, m_table->newCondition(), std::make_shared<AccessOptions>(origin));
        }
        if (count == storage::CODE_NO_AUTHORIZED)
        {
            BOOST_THROW_EXCEPTION(PrecompiledException(std::string("permission denied")));
        }
        out = abi.abiIn("", s256(count));
    }
    else if (func == name2Selector[SIMPLETABLE_METHOD_NEWENT])
    {  // newEntry()
        auto entry = m_table->newEntry();
        auto entryPrecompiled = std::make_shared<EntryPrecompiled>();
        entryPrecompiled->setEntry(entry);

        auto newAddress = context->registerPrecompiled(entryPrecompiled);
        out = abi.abiIn("", newAddress);
    }
    else
    {
        PRECOMPILED_LOG(ERROR) << LOG_BADGE("SimpleTablePrecompiled")
                               << LOG_DESC("call undefined function!");
    }
    return out;
}

h256 SimpleTablePrecompiled::hash()
{
    return m_table->hash();
}