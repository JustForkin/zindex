#include "Sqlite.h"

#include "catch.hpp"

#include <string>
#include <vector>
#include "SqliteError.h"
#include "TempDir.h"

TEST_CASE("opens databases", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";

    SECTION("creates a new db") {
        sqlite.open(dbPath, false);
    }
    SECTION("fails to open a non-existent db") {
        auto threw = false;
        try {
            sqlite.open(dbPath, true);
        } catch (...) {
            threw = true;
        }
        REQUIRE(threw);
    }

    SECTION("reopens a new db") {
        sqlite.open(dbPath, false);
        sqlite.close();
        Sqlite anotherSqlite;
        anotherSqlite.open(dbPath, true);
    }
}

TEST_CASE("prepares statements", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    auto s = sqlite.prepare("create table t(one varchar(10), two integer)");

    SECTION("handles exceptions") {
        std::string threw;
        try {
            sqlite.prepare("SELECT * FROM foo");
        } catch (const SqliteError &e) {
            threw = e.what();
        }
        REQUIRE(threw.find("SQL logic error") != std::string::npos);
    }
}

TEST_CASE("executes statements", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    REQUIRE(sqlite.prepare("create table t(one varchar(10), two integer)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('moo', 1)").step() == true);
    REQUIRE(sqlite.prepare("insert into t values('foo', 2)").step() == true);
    auto select = sqlite.prepare("select * from t order by two desc");
    REQUIRE(select.columnCount() == 2);
    int num = 0;
    for (;;) {
        if (select.step()) break;
        REQUIRE(select.columnName(0) == "one");
        REQUIRE(select.columnName(1) == "two");
        if (num == 0) {
            REQUIRE(select.columnString(0) == "foo");
            REQUIRE(select.columnInt64(1) == 2);
        } else if (num == 1) {
            REQUIRE(select.columnString(0) == "moo");
            REQUIRE(select.columnInt64(1) == 1);
        }
        ++num;
    }
    REQUIRE(num == 2);
}

TEST_CASE("handles binds and blobs", "[Sqlite]") {
    TempDir tempDir;
    Sqlite sqlite;
    auto dbPath = tempDir.path + "/db.sqlite";
    sqlite.open(dbPath, false);

    REQUIRE(sqlite.prepare("create table t(offset integer, data blob)").step() == true);
    auto inserter = sqlite.prepare("insert into t values(:id, :data)");
    inserter.bindInt64(":id", 1234);
    constexpr auto byteLen = 1024;
    char bytes[byteLen];
    for (int i = 0; i < byteLen; ++i) bytes[i] = i & 0xff;
    inserter.bindBlob(":data", bytes, byteLen);
    REQUIRE(inserter.step() == true);
    inserter.reset();
    inserter.bindInt64(":id", 5678);
    for (int i = 0; i < byteLen; ++i) bytes[i] = (i & 0xff) ^ 0xff;
    inserter.bindBlob(":data", bytes, byteLen);
    REQUIRE(inserter.step() == true);

    auto select = sqlite.prepare("select * from t order by offset");
    REQUIRE(select.columnCount() == 2);
    REQUIRE(select.step() == false);
    REQUIRE(select.columnName(0) == "offset");
    REQUIRE(select.columnName(1) == "data");
    REQUIRE(select.columnInt64(0) == 1234);
    auto blob1 = select.columnBlob(1);
    REQUIRE(blob1.size() == byteLen);
    for (int i = 0; i < byteLen; ++i) REQUIRE(blob1[i] == (i & 0xff));
    REQUIRE(select.step() == false);
    REQUIRE(select.columnInt64(0) == 5678);
    auto blob2 = select.columnBlob(1);
    REQUIRE(blob2.size() == byteLen);
    for (int i = 0; i < byteLen; ++i) REQUIRE(blob2[i] == (0xff ^ (i & 0xff)));
    REQUIRE(select.step() == true);
}
