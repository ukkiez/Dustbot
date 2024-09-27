#include <string>
#include <fstream>
#include <atomic>
#include <chrono>
#include <csignal>
#include <dpp/dpp.h>
#include <sqlite3.h>
#include <cstdint>
#include <random>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <thread>
#include <functional>

std::atomic<bool> running = true;
httplib::Server httpServer;

void stop() {
	running = false;
	httpServer.stop();
}

int SQLiteCallback (void *unused, int count, char **data, char **columns) {
	return 0;
}

int main (int argc, char *argv[]) {
	std::string app_directory = std::string(argv[0]);
	app_directory.erase(app_directory.find_last_of('/')+1); // Remove /executeable_name from app directory.

	std::string bot_token;
	std::ifstream bot_token_file(app_directory + "../bot_token.txt");
	std::getline(bot_token_file, bot_token);
	dpp::cluster bot(bot_token);

	sqlite3 *discord_db;
	int32_t db_result = sqlite3_open((app_directory + "discord.db").c_str(), &discord_db);
	if (db_result != SQLITE_OK) {
		std::cout << "Failed to create/open discord database." << std::endl;
	}

	std::string sql = "CREATE TABLE IF NOT EXISTS 'Guilds'(id BIGINT PRIMARY KEY, name TEXT NOT NULL, owner_id BIGINT)";
	char* errMsg;

	db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
	if (db_result != SQLITE_OK) {
		std::cerr << "SQL Exec Error: " << errMsg << std::endl;
		sqlite3_free(errMsg);
	}

	sql = "CREATE TABLE IF NOT EXISTS 'Channels'(id BIGINT PRIMARY KEY, name TEXT NOT NULL, guild_id BIGINT)";
	db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
	if (db_result != SQLITE_OK) {
		std::cerr << "SQL Exec Error: " << errMsg << std::endl;
		sqlite3_free(errMsg);
	}

	sql = "DELETE FROM Guilds; DELETE FROM Channels;";
	db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
	if (db_result != SQLITE_OK) {
		std::cerr << "SQL Exec Error: " << errMsg << std::endl;
		sqlite3_free(errMsg);
	}

	bot.on_log(dpp::utility::cout_logger());

	bot.on_guild_create([&sql, &db_result, &discord_db, &errMsg](const dpp::guild_create_t& event) {
		sql = "INSERT OR IGNORE INTO Guilds(id, name, owner_id) VALUES (?,?,?)";
		sqlite3_stmt *stmt = nullptr;
		db_result = sqlite3_prepare_v2(discord_db, sql.c_str(), -1, &stmt, nullptr);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Prepare v2 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_int64(stmt, 1, event.created->id);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_text(stmt, 2, event.created->name.c_str(), -1, SQLITE_STATIC);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_int64(stmt, 3, event.created->owner_id);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_step(stmt);
		if (db_result != SQLITE_DONE) {
			std::cerr << "SQL Step Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		sqlite3_finalize(stmt);
		for (uint64_t channel_id : event.created->channels) {
			dpp::channel *channel = dpp::find_channel(channel_id);
			if (channel) {
				sql = "INSERT OR IGNORE INTO Channels(id, name, guild_id) VALUES (?,?,?)";
				sqlite3_stmt *stmt = nullptr;
				db_result = sqlite3_prepare_v2(discord_db, sql.c_str(), -1, &stmt, nullptr);
				if (db_result != SQLITE_OK) {
					std::cerr << "SQL Prepare v2 Error: " << sqlite3_errmsg(discord_db) << std::endl;
					sqlite3_free(errMsg);
				}
				db_result = sqlite3_bind_int64(stmt, 1, channel_id);
				if (db_result != SQLITE_OK) {
					std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
					sqlite3_free(errMsg);
				}
				db_result = sqlite3_bind_text(stmt, 2, channel->name.c_str(), -1, SQLITE_STATIC);
				if (db_result != SQLITE_OK) {
					std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
					sqlite3_free(errMsg);
				}
				db_result = sqlite3_bind_int64(stmt, 3, channel->guild_id);
				if (db_result != SQLITE_OK) {
					std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
					sqlite3_free(errMsg);
				}
				db_result = sqlite3_step(stmt);
				if (db_result != SQLITE_DONE) {
					std::cerr << "SQL Step Error: " << sqlite3_errmsg(discord_db) << std::endl;
					sqlite3_free(errMsg);
				}
				sqlite3_finalize(stmt);
			}
		}
	});

	bot.on_guild_delete([&sql, &db_result, &discord_db, &errMsg](const dpp::guild_delete_t& event) {
		sql = "DELETE FROM Guilds WHERE id=" + std::to_string(event.deleted.id);
		db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Exec Error: " << errMsg << std::endl;
			sqlite3_free(errMsg);
		}
		sql = "DELETE FROM Channels WHERE guild_id=" + std::to_string(event.deleted.id);
		db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Exec Error: " << errMsg << std::endl;
			sqlite3_free(errMsg);
		}
	});

	bot.on_channel_create([&sql, &db_result, &discord_db, &errMsg](const dpp::channel_create_t& event) {
		sql = "INSERT OR IGNORE INTO Channels(id, name, guild_id) VALUES (?,?,?)";
		sqlite3_stmt *stmt = nullptr;
		db_result = sqlite3_prepare_v2(discord_db, sql.c_str(), -1, &stmt, nullptr);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Prepare v2 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_int64(stmt, 1, event.created->id);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_text(stmt, 2, event.created->name.c_str(), -1, SQLITE_STATIC);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_bind_int64(stmt, 3, event.created->guild_id);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		db_result = sqlite3_step(stmt);
		if (db_result != SQLITE_DONE) {
			std::cerr << "SQL Step Error: " << sqlite3_errmsg(discord_db) << std::endl;
			sqlite3_free(errMsg);
		}
		sqlite3_finalize(stmt);
	});

	bot.on_channel_delete([&sql, &db_result, &discord_db, &errMsg](const dpp::channel_delete_t& event) {
		sql = "DELETE FROM Channels WHERE id=" + std::to_string(event.deleted.id);
		db_result = sqlite3_exec(discord_db, sql.c_str(), SQLiteCallback, NULL, &errMsg);
		if (db_result != SQLITE_OK) {
			std::cerr << "SQL Exec Error: " << errMsg << std::endl;
			sqlite3_free(errMsg);
		}
	});

	std::cout << "Starting bot." << std::endl;
	bot.start(dpp::st_return);
	
	std::string oauth_token;
	std::ifstream oauth_token_file(app_directory + "../oauth_token.txt");
	std::getline(oauth_token_file, oauth_token);

	httpServer.Get("/", [&app_directory, &oauth_token](const httplib::Request &req, httplib::Response &res) {
		std::ifstream file(app_directory + "../web/index.html");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (req.has_param("code")) {
			std::string code = req.get_param_value("code");
			httplib::Params params = {
				{ "grant_type", "authorization_code" },
				{ "code", code },
				{ "authorization_url", "http://localhost:8080/" }
			};
			std::string paramsString = "";
			bool first = true;
			for (std::pair<std::string, std::string> param : params) {
				if (first) {
					paramsString += param.first + "=" + param.second;
					first = false;
				} else {
					paramsString += "&" + param.first + "=" + param.second;
				}
			}
			std::cout << paramsString << std::endl;
			httplib::SSLClient httpClient("discord.com", 443);
			httpClient.set_basic_auth("1234389167576977458", oauth_token);
			httplib::Result result = httpClient.Post("/api/oauth2/token", paramsString, "application/x-www-form-urlencoded");
			if (result) {
				std::cout << result->status << std::endl;
				std::cout << result->body << std::endl;
			} else {
				std::cerr << "Error doing post: " << result.error() << std::endl;
			}
		}
		res.set_content(content, "text/html");
	});

	httpServer.Get("/style.css", [&app_directory](const httplib::Request &req, httplib::Response &res) {
		std::ifstream file(app_directory + "../web/style.css");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		res.set_content(content, "text/css");
	});

	std::signal(SIGINT, [](int code) {
		std::cout << "Got shutdown signal..." << std::endl;
		stop();
	}); // Catch Ctrl+C

	std::cout << "Starting httpServer." << std::endl;
	std::thread httpThread([]() {
		httpServer.listen("localhost", 8080);
	});
	httpThread.detach();

	while (running) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	sqlite3_close(discord_db);

	return 0;
}
