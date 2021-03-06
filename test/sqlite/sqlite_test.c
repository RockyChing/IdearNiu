#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>


#include <sqlite3.h>
#include <type_def.h>
#include <log_util.h>

#define SQLITE_DB_NAME "./resource/testDB.db"
/* Create SQL statement */
#define SQL_CREATE_TBL "CREATE TABLE COMPANY("  \
         "id INT PRIMARY KEY     NOT NULL," \
         "name           TEXT    NOT NULL," \
         "age            INT     NOT NULL," \
         "address        CHAR(50)," \
         "salary         REAL );"

static int callback(void *NotUsed, int argc, char **argv, char **azColName){
   int i;
   for(i = 0; i < argc; i ++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }

   printf("\n");
   return 0;
}

static int sqlite_tbl_create(sqlite3 *db, const char *sql)
{
	char *err_msg;
	int ret;
	assert_param(db);
	assert_param(sql);
	ret = sqlite3_exec(db, sql, callback, 0, &err_msg);
	if (ret != SQLITE_OK ){
		sys_debug(0, "sqlite_tbl_create error: %s\n", err_msg);
		sqlite3_free(err_msg);
		return -1;
	}

	sys_debug(0, "Table created successfully\n");
	return 0;
}

static int sqlite_tbl_exec(sqlite3 *db, const char *sql)
{
	char *err_msg;
	int ret;
	assert_param(db);
	assert_param(sql);
	ret = sqlite3_exec(db, sql, callback, 0, &err_msg);
	if (ret != SQLITE_OK ){
		sys_debug(0, "sqlite_tbl_exec error: %s\n", err_msg);
		sqlite3_free(err_msg);
		return -1;
	} else {
		sys_debug(0, "done '%s'\n", sql);
	}

	return 0;
}

static void sqlite_insert_test(sqlite3 *db)
{
	char *sql = "INSERT INTO COMPANY (id, name, age, address, salary) VALUES (%d, 'A%d', %d, 'Norway', %d)";
	char sql_buf[255] = {0};
	int i;
	for (i = 0; i < 9999; i ++) {
		snprintf(sql_buf, sizeof(sql_buf), sql, i, i, i, i);
		sqlite_tbl_exec(db, sql_buf);
		memset(sql_buf, 0, sizeof(sql_buf));
		usleep(100 * 1000);
	}
}

static void sqlite_select_test(sqlite3 *db)
{
	const char *sql = "select * from COMPANY where id = 3799";
	sqlite_tbl_exec(db, sql);
}


void sqlite_test_entry()
{
	func_enter();
	sqlite3 *db;
	//char *err;
	int ret;

	ret = sqlite3_open(SQLITE_DB_NAME, &db);
	if (ret) {
		sys_debug(0, "Opened database failed: %s\n", sqlite3_errmsg(db));
		exit(0);
	}
	sys_debug(0, "Opened database successfully\n");
	//ret = sqlite_tbl_create(db, SQL_CREATE_TBL);
	//sqlite_insert_test(db);
	sqlite_select_test(db);

	sqlite3_close(db);
	func_exit();
}


