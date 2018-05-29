
#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

/* placeholders; will be undef'd and redef'd */
#define SQL_VAL
#define SQL /* attached for troubleshooting purposes */

namespace test {

class ConvertSQL2C_Interval : public ::testing::Test, public ConnectedDBC {

  protected:
    SQLRETURN ret;
    SQL_INTERVAL_STRUCT is;
    SQLLEN ind_len = SQL_NULL_DATA;

  ConvertSQL2C_Interval() {
  }

  virtual ~ConvertSQL2C_Interval() {
  }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  void prepareStatement(const SQLWCHAR *sql, const char *json_answer) {
    char *answer = STRDUP(json_answer);
    ASSERT_TRUE(answer != NULL);
    ret =  ATTACH_ANSWER(stmt, answer, strlen(answer));
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
    ret = ATTACH_SQL(stmt, sql, wcslen(sql));
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    ret = SQLBindCol(stmt, /*col#*/1, SQL_C_INTERVAL_HOUR, &is, sizeof(is),
        &ind_len);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};



TEST_F(ConvertSQL2C_Interval, Integer2Interval_unsupported_HYC00) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "1"
#define SQL   "select " SQL_VAL

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"select " SQL "\", \"type\": \"integer\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"HYC00");
}


TEST_F(ConvertSQL2C_Interval, Integer2Interval_violation_07006) {

#undef SQL_VAL
#undef SQL
#define SQL_VAL "1.1"
#define SQL   "select " SQL_VAL

  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"select " SQL "\", \"type\": \"double\"}\
  ],\
  \"rows\": [\
    [" SQL_VAL "]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));
  assertState(L"07006");
}


} // test namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
