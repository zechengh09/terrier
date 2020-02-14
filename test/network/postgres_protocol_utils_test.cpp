#include <vector>

#include "gtest/gtest.h"
#include "network/postgres/postgres_protocol_utils.h"
#include "test_util/test_harness.h"

namespace terrier::network {

class PostgresProtocolUtilsTests : public TerrierTest {};

// NOLINTNEXTLINE
TEST_F(PostgresProtocolUtilsTests, TypeConversionTest) {
  // Check that we can correctly convert our types back and forth from Postgres types

  // I hate C++ enums so much. Just give me a fucking iterator...
  std::vector<type::TypeId> all_types = {
      type::TypeId::BOOLEAN,
      // TINYINT is aliased to boolean in Postgres, so we'll skip it
      // type::TypeId::TINYINT,
      type::TypeId::SMALLINT,
      type::TypeId::INTEGER,
      type::TypeId::BIGINT,
      type::TypeId::DECIMAL,
      type::TypeId::TIMESTAMP,
      type::TypeId::DATE,
      type::TypeId::VARCHAR,
      type::TypeId::VARBINARY,
  };
  for (type::TypeId orig_internal_type : all_types) {
    auto postgres_type = InternalValueTypeToPostgresValueType(orig_internal_type);
    auto internal_type = PostgresValueTypeToInternalValueType(postgres_type);

    EXPECT_NE(PostgresValueType::INVALID, postgres_type);
    EXPECT_EQ(internal_type, orig_internal_type);
  }

  // Check that we get an exception if we try to cast something that we don't handle
  EXPECT_THROW(PostgresValueTypeToInternalValueType(PostgresValueType::INVALID), NetworkProcessException);
}

}  // namespace terrier::network
