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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::atomic<bool> running = true;
httplib::Server httpServer;

void stop() {
	running = false;
	httpServer.stop();
}

int SQLiteCallback (void *unused, int count, char **data, char **columns) {
	return 0;
}

std::vector<std::string> split(const std::string& string, const std::string& delimiter) {
    std::vector<std::string> result;
    size_t position = 0;
    size_t nextPos;
    while ((nextPos = string.find(delimiter, position)) != std::string::npos) {
        result.push_back(string.substr(position, nextPos - position));
        position = nextPos + delimiter.size();
    }
    result.push_back(string.substr(position));
    return result;
}

std::string generate_session_token(uint32_t length) {
	std::ifstream urandom("/dev/urandom");
	std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~";
	std::string sessionId;
	sessionId.reserve(length);

	std::random_device rd;
	std::mt19937 generator(rd());
	std::uniform_int_distribution<int> distribution(0, chars.size() - 1);

	for (int i = 0; i < length; ++i) {
		char random_char;
		urandom.read(&random_char, 1);
		int index = distribution(generator) % chars.size();
		sessionId += chars[index];
	}

	urandom.close();
	return sessionId;
}

int main (int argc, char *argv[]) {
	std::string app_directory = std::string(argv[0]);
	app_directory.erase(app_directory.find_last_of('/') + 1); // Remove /executeable_name from app directory.

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

	sql = std::string("CREATE TABLE IF NOT EXISTS 'OAuth2Users'(") +
		"id BIGINT PRIMARY KEY," +
		"name TEXT NOT NULL," +
		"access_token TEXT NOT NULL," +
		"token_type TEXT NOT NULL," +
		"expiry BIGINT NOT NULL," +
		"refresh_token TEXT NOT NULL," +
		"scope TEXT NOT NULL," +
		"session_token NOT NULL" +
	")";
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

	httpServer.Get("/", [&app_directory](const httplib::Request &req, httplib::Response &res) {
		std::ifstream file(app_directory + "../web/index.html");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		res.set_content(content, "text/html");
	});

	httpServer.Get("/style.css", [&app_directory](const httplib::Request &req, httplib::Response &res) {
		std::ifstream file(app_directory + "../web/style.css");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		res.set_content(content, "text/css");
	});

	httpServer.Get("/main.js", [&app_directory](const httplib::Request &req, httplib::Response &res) {
		std::ifstream file(app_directory + "../web/main.js");
		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		res.set_content(content, "application/javascript");
	});

	httpServer.Get("/json", [&discord_db, &sql, &errMsg, &db_result](const httplib::Request &req, httplib::Response &res) {
		json content = { };
		content["verified"] = false;
		uint64_t discord_id = 0;
		std::string session_token = "";
		if (req.has_header("Cookie")) {
			std::string cookieHeader = req.get_header_value("Cookie");
			std::vector<std::string> cookies = split(cookieHeader, "; ");
			for (std::string &cookie : cookies) {
				std::string key, value;
				uint32_t index = cookie.find("=");
				key = cookie.substr(0, index);
				value = cookie.substr(index + 1);
				if (key == "discord_id") {
					try {
						discord_id = std::stoull(value);
					} catch (std::invalid_argument &e) {
						std::cerr << "Invalid argument: " << e.what() << std::endl;
					} catch (std::out_of_range &e) {
						std::cerr << "Out of range: " << e.what() << std::endl;
					}
				}
				if (key == "session_token") {
					session_token = value;
				}
			}
		}
		if (discord_id != 0 && session_token != "") {
			sql = "SELECT session_token FROM OAuth2Users WHERE id=?";
			sqlite3_stmt *stmt = nullptr;

			db_result = sqlite3_prepare_v2(discord_db, sql.c_str(), -1, &stmt, nullptr);
			if (db_result != SQLITE_OK) {
				std::cerr << "SQL Prepare v2 Error: " << sqlite3_errmsg(discord_db) << std::endl;
				sqlite3_free(errMsg);
			}

			db_result = sqlite3_bind_int64(stmt, 1, discord_id);
			if (db_result != SQLITE_OK) {
				std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
				sqlite3_free(errMsg);
			}

			if (sqlite3_step(stmt) == SQLITE_ROW) {
				std::string db_session_token = (const char*)sqlite3_column_text(stmt, 0);
				if (session_token == db_session_token) {
					content["verified"] = true;
				}
			}

			sqlite3_finalize(stmt);
		}
		res.set_content(content.dump(), "application/json");
	});

	httpServer.Get("/discord-login", [&oauth_token, &sql, &discord_db, &db_result, &errMsg](const httplib::Request &req, httplib::Response &res) {
		std::string session_token = "";
		uint64_t discord_id;
		if (req.has_param("code")) {
			std::string code = req.get_param_value("code");
			httplib::Params params = {
				{ "grant_type", "authorization_code" },
				{ "code", code },
				{ "redirect_uri", "https://dustbot.xyz/discord-login" }
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
			httplib::SSLClient tokenClient("discord.com", 443);
			tokenClient.set_basic_auth("1234389167576977458", oauth_token);
			httplib::Result getTokenResult = tokenClient.Post("/api/v10/oauth2/token", paramsString, "application/x-www-form-urlencoded");
			if (getTokenResult) {
				json getTokenJson = json::parse(getTokenResult->body);
				if (getTokenJson.contains("access_token")) {
					httplib::SSLClient bearerClient("discord.com", 443);
					bearerClient.set_bearer_token_auth(getTokenJson["access_token"]);
					httplib::Result getUserResult = bearerClient.Get("/api/v10/users/@me");
					json getUserJson = json::parse(getUserResult->body);
					sql = "INSERT OR REPLACE INTO OAuth2Users(id, name, access_token, token_type, expiry, refresh_token, scope, session_token) VALUES (?,?,?,?,?,?,?,?)";
					sqlite3_stmt *stmt = nullptr;
					db_result = sqlite3_prepare_v2(discord_db, sql.c_str(), -1, &stmt, nullptr);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Prepare v2 Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					discord_id = std::stoull(getUserJson["id"].get<std::string>());
					db_result = sqlite3_bind_int64(stmt, 1, discord_id);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_bind_text(stmt, 2, getUserJson["username"].get<std::string>().c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_bind_text(stmt, 3, getTokenJson["access_token"].get<std::string>().c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_bind_text(stmt, 4, getTokenJson["token_type"].get<std::string>().c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					std::chrono::time_point now = std::chrono::system_clock::now();
					std::chrono::seconds time_in_seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
					int64_t token_expiry = time_in_seconds.count() + getTokenJson["expires_in"].get<int64_t>();
					db_result = sqlite3_bind_int64(stmt, 5, token_expiry);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind int64 Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_bind_text(stmt, 6, getTokenJson["refresh_token"].get<std::string>().c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_bind_text(stmt, 7, getTokenJson["scope"].get<std::string>().c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					session_token = generate_session_token(128);
					db_result = sqlite3_bind_text(stmt, 8, session_token.c_str(), -1, SQLITE_STATIC);
					if (db_result != SQLITE_OK) {
						std::cerr << "SQL Bind Text Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					db_result = sqlite3_step(stmt);
					if (db_result != SQLITE_DONE) {
						std::cerr << "SQL Step Error: " << sqlite3_errmsg(discord_db) << std::endl;
						sqlite3_free(errMsg);
					}
					sqlite3_finalize(stmt);
				}
			} else {
				std::cerr << "Error doing get: " << getTokenResult.error() << std::endl;
			}
		}
		if (session_token != "") {
			res.set_header("Set-Cookie", "discord_id=" + std::to_string(discord_id) + "; Path=/; Secure; HttpOnly");
			res.set_header("Set-Cookie", "session_token=" + session_token + "; Path=/; Secure; HttpOnly");
		}
		res.set_redirect("/");
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
