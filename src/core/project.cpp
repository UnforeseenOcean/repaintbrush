#include "project.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace core {
    void do_simple_statement(sqlite3* db, const std::string& stmt_str)
    {
        sqlite3_stmt* statement;
        int ok = sqlite3_prepare_v2(db,
            stmt_str.c_str(), stmt_str.size(), &statement, nullptr);
        if (ok != SQLITE_OK) {
            auto err = std::runtime_error(sqlite3_errmsg(db));
            sqlite3_finalize(statement);
            throw err;
        }
        ok = sqlite3_step(statement);
        if (ok != SQLITE_DONE) {
            auto err = std::runtime_error(sqlite3_errmsg(db));
            sqlite3_finalize(statement);
            throw err;
        }
        ok = sqlite3_finalize(statement);
        if (ok != SQLITE_OK) {
            auto err = std::runtime_error(sqlite3_errmsg(db));
            throw err;
        }
    }

    Project::Project(Glib::RefPtr<Gio::File>& path, bool force, int flags)
    : m_lock(std::make_shared<ProjectFolderLock>(path, force))
    , m_path(path)
    , m_database(path->get_child(rbrush_folder_name)->get_child(rbrush_db_name)
        ->get_path(), flags) {}

    Project Project::connect(Glib::RefPtr<Gio::File>& path, bool force)
    {
        return Project(path, force, SQLITE_OPEN_READWRITE);
    }

    Project Project::create(Glib::RefPtr<Gio::File>& path, bool force)
    {
        Project project(path, force, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        {
            auto stmt = project.get_database().prepare(
                R"(CREATE TABLE IF NOT EXISTS images(
                    id INT PRIMARY KEY NOT NULL,
                    name NTEXT NOT NULL,
                    alias NTEXT
                ))"
            );
            stmt.finish();
        }
        {
            auto stmt = project.get_database().prepare(
                R"(CREATE TABLE IF NOT EXISTS inputfolders(
                    name TEXT NOT NULL UNIQUE
                ))"
            );
            stmt.finish();
        }
        return project;
    }

    database::Database& Project::get_database()
    {
        return this->m_database;
    }

    bool Project::add_inputfolder(Glib::RefPtr<Gio::File>& path)
    {
        if (path->query_file_type() != Gio::FileType::FILE_TYPE_DIRECTORY) {
            std::stringstream s;
            s << "Error: '" << path->get_path()
                << "' is not a valid directory!";
            throw std::runtime_error(s.str());
        }
        auto& db = this->get_database();
        auto stmt = db.prepare(
            R"(INSERT INTO inputfolders(name)
            VALUES (?))"
        );
        stmt.bind(1, path->get_path());
        try {
            stmt.finish();
        } catch (std::exception& e) {
            if (std::string(e.what()) ==
                    "UNIQUE constraint failed: inputfolders.name") {
                return true;
            } else {
                throw e;
            }
        }
        return false;
    }

    bool Project::remove_inputfolder(Glib::RefPtr<Gio::File>& path)
    {
        auto& db = this->get_database();
        auto stmt = db.prepare(
            R"(DELETE FROM inputfolders
            WHERE name = ?)"
        );
        stmt.bind(1, path->get_path());
        stmt.finish();
        // std::cout << sqlite3_changes(db.get_ptr())
        // << " rows were deleted." << std::endl;
        return sqlite3_changes(db.get_ptr()) == 0;
    }

    std::vector<std::string> Project::list_input_folders()
    {
        std::vector<std::string> ret;
        auto& db = this->get_database();
        auto stmt = db.prepare(
            R"(SELECT name FROM inputfolders)"
        );
        while (!stmt.done()) {
            if (stmt.step() == SQLITE_ROW) {
                ret.push_back(stmt.column_value<std::string>(1));
            }
        }
        return ret;
    }
}
