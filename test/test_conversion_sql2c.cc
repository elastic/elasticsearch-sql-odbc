

#include <gtest/gtest.h>
#include "connected_dbc.h"

#include <string.h>

#define SQL /* placeholder; SQL is attached just for debugging */

namespace test {

class ConvertSQL2C_Timestamp : public ::testing::Test, public ConnectedDBC {

  protected:
    SQLRETURN ret;
    TIMESTAMP_STRUCT ts;
    SQLLEN ind_len = SQL_NULL_DATA;

  ConvertSQL2C_Timestamp() {
  }

  virtual ~ConvertSQL2C_Timestamp() {
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

    ret = SQLBindCol(stmt, /*col#*/1, SQL_C_TYPE_TIMESTAMP, &ts, sizeof(ts),
        &ind_len);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));
  }
};


/* ES/SQL 'date' is actually 'timestamp' */
TEST_F(ConvertSQL2C_Timestamp, Timestamp2Timestamp_noTruncate) {

#undef SQL
#define SQL "CAST(2345-01-23T12:34:56.789Z)"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"date\"}\
  ],\
  \"rows\": [\
    [\"2345-01-23T12:34:56.789Z\"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ts));
  EXPECT_EQ(ts.year, 2345);
  EXPECT_EQ(ts.month, 1);
  EXPECT_EQ(ts.day, 23);
  EXPECT_EQ(ts.hour, 12);
  EXPECT_EQ(ts.minute, 34);
  EXPECT_EQ(ts.second, 56);
  EXPECT_EQ(ts.fraction, 789);
}
/* No second fraction truncation done by the driver -> no test for 01S07 */

TEST_F(ConvertSQL2C_Timestamp, Timestamp2Timestamp_trimming) {

#undef SQL
#define SQL "CAST(   2345-01-23T12:34:56.789Z  )"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"date\"}\
  ],\
  \"rows\": [\
    [\"   2345-01-23T12:34:56.789Z  \"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ts));
  EXPECT_EQ(ts.year, 2345);
  EXPECT_EQ(ts.month, 1);
  EXPECT_EQ(ts.day, 23);
  EXPECT_EQ(ts.hour, 12);
  EXPECT_EQ(ts.minute, 34);
  EXPECT_EQ(ts.second, 56);
  EXPECT_EQ(ts.fraction, 789);
}


TEST_F(ConvertSQL2C_Timestamp, Date2Timestamp) {

#undef SQL
#define SQL "CAST(2345-01-23T)"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"2345-01-23T\"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ts));
  EXPECT_EQ(ts.year, 2345);
  EXPECT_EQ(ts.month, 1);
  EXPECT_EQ(ts.day, 23);
  EXPECT_EQ(ts.hour, 0);
  EXPECT_EQ(ts.minute, 0);
  EXPECT_EQ(ts.second, 0);
  EXPECT_EQ(ts.fraction, 0);
}


TEST_F(ConvertSQL2C_Timestamp, Time2Timestamp) {

#undef SQL
#define SQL "CAST(10:10:10.1010)"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"10:10:10.1010\"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ts));
  EXPECT_EQ(ts.year, 0);
  EXPECT_EQ(ts.month, 0);
  EXPECT_EQ(ts.day, 0);
  EXPECT_EQ(ts.hour, 10);
  EXPECT_EQ(ts.minute, 10);
  EXPECT_EQ(ts.second, 10);
  EXPECT_EQ(ts.fraction, 101);
}


TEST_F(ConvertSQL2C_Timestamp, Time2Timestamp_trimming) {

#undef SQL
#define SQL "CAST(  10:10:10.1010   )"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"text\"}\
  ],\
  \"rows\": [\
    [\"  10:10:10.1010   \"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(ind_len, sizeof(ts));
  EXPECT_EQ(ts.year, 0);
  EXPECT_EQ(ts.month, 0);
  EXPECT_EQ(ts.day, 0);
  EXPECT_EQ(ts.hour, 10);
  EXPECT_EQ(ts.minute, 10);
  EXPECT_EQ(ts.second, 10);
  EXPECT_EQ(ts.fraction, 101);
}


TEST_F(ConvertSQL2C_Timestamp, String2Timestamp_invalidFormat_22018) {

#undef SQL
#define SQL "CAST(invalid 2345-01-23T12:34:56.789Z)"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"date\"}\
  ],\
  \"rows\": [\
    [\"invalid 2345-01-23T12:34:56.789Z\"]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));

  SQLWCHAR state[SQL_SQLSTATE_SIZE+1] = {L'\0'};
  SQLSMALLINT len;
  ret = SQLGetDiagField(SQL_HANDLE_STMT, stmt, 1, SQL_DIAG_SQLSTATE, state,
      (SQL_SQLSTATE_SIZE + 1) * sizeof(state[0]), &len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ASSERT_EQ(len, SQL_SQLSTATE_SIZE * sizeof(state[0]));
  ASSERT_STREQ(state, L"22018");
}

TEST_F(ConvertSQL2C_Timestamp, Integer2Timestamp_violation_07006) {

#undef SQL
#define SQL "select 1"

  const SQLWCHAR *sql = MK_WPTR(SQL);
  const char json_answer[] = "\
{\
  \"columns\": [\
    {\"name\": \"" SQL "\", \"type\": \"integer\"}\
  ],\
  \"rows\": [\
    [1]\
  ]\
}\
";
  prepareStatement(MK_WPTR(SQL), json_answer);

  ret = SQLFetch(stmt);
  ASSERT_FALSE(SQL_SUCCEEDED(ret));

  SQLWCHAR state[SQL_SQLSTATE_SIZE+1] = {L'\0'};
  SQLSMALLINT len;
  ret = SQLGetDiagField(SQL_HANDLE_STMT, stmt, 1, SQL_DIAG_SQLSTATE, state,
      (SQL_SQLSTATE_SIZE + 1) * sizeof(state[0]), &len);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  ASSERT_EQ(len, SQL_SQLSTATE_SIZE * sizeof(state[0]));
  ASSERT_STREQ(state, L"07006");
}


} // test namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
