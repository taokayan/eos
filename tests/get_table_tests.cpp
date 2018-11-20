#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/wasm_eosio_constraints.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>

#include <asserter/asserter.wast.hpp>
#include <asserter/asserter.abi.hpp>

#include <stltest/stltest.wast.hpp>
#include <stltest/stltest.abi.hpp>

#include <eosio.system/eosio.system.wast.hpp>
#include <eosio.system/eosio.system.abi.hpp>

#include <eosio.token/eosio.token.wast.hpp>
#include <eosio.token/eosio.token.abi.hpp>

#include <fc/io/fstream.hpp>

#include <Runtime/Runtime.h>

#include <fc/variant_object.hpp>
#include <fc/io/json.hpp>

#include <array>
#include <utility>

#ifdef NON_VALIDATING_TEST
#define TESTER tester
#else
#define TESTER validating_tester
#endif

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;

BOOST_AUTO_TEST_SUITE(get_table_tests)

BOOST_FIXTURE_TEST_CASE( get_scope_test, TESTER ) try {
   produce_blocks(2);

   create_accounts({ N(eosio.token), N(eosio.ram), N(eosio.ramfee), N(eosio.stake),
      N(eosio.bpay), N(eosio.vpay), N(eosio.saving), N(eosio.names) });

   std::vector<account_name> accs{N(inita), N(initb), N(initc), N(initd)};
   create_accounts(accs);
   produce_block();

   set_code( N(eosio.token), eosio_token_wast );
   set_abi( N(eosio.token), eosio_token_abi );
   produce_blocks(1);

   // create currency 
   auto act = mutable_variant_object()
         ("issuer",       "eosio")
         ("maximum_supply", eosio::chain::asset::from_string("1000000000.0000 SYS"));
   push_action(N(eosio.token), N(create), N(eosio.token), act );

   // issue
   for (account_name a: accs) {
      push_action( N(eosio.token), N(issue), "eosio", mutable_variant_object()
                  ("to",      name(a) )
                  ("quantity", eosio::chain::asset::from_string("999.0000 SYS") )
                  ("memo", "")
                  );
   }
   produce_blocks(1);

   // iterate over scope
   eosio::chain_apis::read_only plugin(*(this->control), fc::microseconds(INT_MAX));
   eosio::chain_apis::read_only::get_table_by_scope_params param{N(eosio.token), N(accounts), "inita", "", 10};
   eosio::chain_apis::read_only::get_table_by_scope_result result = plugin.read_only::get_table_by_scope(param);

   BOOST_REQUIRE_EQUAL(4, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 4) {
      BOOST_REQUIRE_EQUAL(name(N(eosio.token)), result.rows[0].code);
      BOOST_REQUIRE_EQUAL(name(N(inita)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(accounts)), result.rows[0].table);
      BOOST_REQUIRE_EQUAL(name(N(eosio)), result.rows[0].payer);
      BOOST_REQUIRE_EQUAL(1, result.rows[0].count);

      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[1].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[2].scope);
      BOOST_REQUIRE_EQUAL(name(N(initd)), result.rows[3].scope);
   }

   param.lower_bound = "initb";
   param.upper_bound = "initd";
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(2, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more);
   if (result.rows.size() >= 2) {
      BOOST_REQUIRE_EQUAL(name(N(initb)), result.rows[0].scope);
      BOOST_REQUIRE_EQUAL(name(N(initc)), result.rows[1].scope);      
   }

   param.limit = 1;
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = name(0);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(1, result.rows.size());
   BOOST_REQUIRE_EQUAL("initc", result.more);

   param.table = N(invalid);
   result = plugin.read_only::get_table_by_scope(param);
   BOOST_REQUIRE_EQUAL(0, result.rows.size());
   BOOST_REQUIRE_EQUAL("", result.more); 

} FC_LOG_AND_RETHROW() /// get_scope_test

BOOST_FIXTURE_TEST_CASE( key_convertion_test, TESTER ) try {

   // decimal to uint64_t
   BOOST_REQUIRE_EQUAL(0, chain_apis::convert_to_type<uint64_t>("0", ""));
   BOOST_REQUIRE_EQUAL(0, chain_apis::convert_to_type<uint64_t>("00000", ""));
   BOOST_REQUIRE_EQUAL(5, chain_apis::convert_to_type<uint64_t>("5", ""));
   BOOST_REQUIRE_EQUAL(111111111111ull, chain_apis::convert_to_type<uint64_t>("111111111111", ""));
   BOOST_REQUIRE_EQUAL(2222222222222ull, chain_apis::convert_to_type<uint64_t>("2222222222222", ""));
   BOOST_REQUIRE_EQUAL(LLONG_MAX, chain_apis::convert_to_type<uint64_t>("9223372036854775807", ""));
   BOOST_REQUIRE_EQUAL(LLONG_MAX+1, chain_apis::convert_to_type<uint64_t>("9223372036854775808", ""));
   BOOST_REQUIRE_EQUAL(ULLONG_MAX-1, chain_apis::convert_to_type<uint64_t>("18446744073709551614", ""));
   BOOST_REQUIRE_EQUAL(ULLONG_MAX, chain_apis::convert_to_type<uint64_t>("18446744073709551615", ""));

   // string to name
   BOOST_REQUIRE_EQUAL(0, chain_apis::convert_to_type<uint64_t>("", ""));
   BOOST_REQUIRE_EQUAL(N(.a), chain_apis::convert_to_type<uint64_t>(".a", ""));
   BOOST_REQUIRE_EQUAL(N(.a..1), chain_apis::convert_to_type<uint64_t>(".a..1", ""));
   BOOST_REQUIRE_EQUAL(N(a), chain_apis::convert_to_type<uint64_t>("a", ""));
   BOOST_REQUIRE_EQUAL(N(a.com), chain_apis::convert_to_type<uint64_t>("a.com", ""));
   BOOST_REQUIRE_EQUAL(N(a), chain_apis::convert_to_type<uint64_t>(" a", ""));
   BOOST_REQUIRE_EQUAL(N(3), chain_apis::convert_to_type<uint64_t>(" 3", ""));
   BOOST_REQUIRE_EQUAL(N(111111111111), chain_apis::convert_to_type<uint64_t>(" 111111111111", ""));
   BOOST_REQUIRE_EQUAL(N(2222222222222), chain_apis::convert_to_type<uint64_t>(" 2222222222222", ""));
   BOOST_REQUIRE_EQUAL(N(11111111111.1), chain_apis::convert_to_type<uint64_t>("11111111111.1", ""));
   BOOST_REQUIRE_EQUAL(N(11111111111.5), chain_apis::convert_to_type<uint64_t>("11111111111.5", ""));
   BOOST_REQUIRE_EQUAL(N(11111111111.a), chain_apis::convert_to_type<uint64_t>("11111111111.a", ""));
   BOOST_REQUIRE_EQUAL(N(12345abcdefg), chain_apis::convert_to_type<uint64_t>("12345abcdefg", ""));
   BOOST_REQUIRE_EQUAL(N(hijklmnopqrs), chain_apis::convert_to_type<uint64_t>("hijklmnopqrs", ""));
   BOOST_REQUIRE_EQUAL(N(tuvwxyz.1234), chain_apis::convert_to_type<uint64_t>("tuvwxyz.1234", ""));

   BOOST_REQUIRE_EQUAL(SY(4,EOS), chain_apis::convert_to_type<uint64_t>("4,EOS", ""));
   BOOST_REQUIRE_EQUAL(SY(0,AAA), chain_apis::convert_to_type<uint64_t>("0,AAA", ""));
   BOOST_REQUIRE_EQUAL(SY(18,ABCDEFG), chain_apis::convert_to_type<uint64_t>("18,ABCDEFG", ""));
   BOOST_REQUIRE_EQUAL(SY(18,ZZZZZZZ), chain_apis::convert_to_type<uint64_t>("18,ZZZZZZZ", ""));
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("19,ABCDEFG", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("4,ABCDEFGH", ""), chain_type_exception);

   BOOST_REQUIRE_EQUAL(SY(0,EOS)>>8, chain_apis::convert_to_type<uint64_t>("EOS", ""));
   BOOST_REQUIRE_EQUAL(SY(0,ABCDEFG)>>8, chain_apis::convert_to_type<uint64_t>("ABCDEFG", ""));
   BOOST_REQUIRE_EQUAL(SY(0,ZZZZZZZ)>>8, chain_apis::convert_to_type<uint64_t>("ZZZZZZZ", ""));

   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("_", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>(" 6", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("0.1", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("Eos", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("EOS123", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("12345abcdefghi", ""), chain_type_exception);
   BOOST_CHECK_THROW(chain_apis::convert_to_type<uint64_t>("18446744073709551616", ""), chain_type_exception);

} FC_LOG_AND_RETHROW() /// key_convertion_test

BOOST_AUTO_TEST_SUITE_END()

