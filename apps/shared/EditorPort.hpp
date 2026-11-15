#pragma once

/// @file
/// @brief The `.nrepl-port` file in an app's folder, where an nREPL client (CIDER, Calva, Conjure) finds the port of
/// the host's editor server (Screen::listenEditor(), `--nrepl`), and the line the clients look for on the terminal.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

/// @brief While it lives, keeps a file `.nrepl-port` in a folder, holding only the port in digits, and removes it when
/// it goes if the file still holds that port (another host may have taken the folder since). It never writes through
/// a symbolic link: it writes a file of its own beside it and renames it over the name, which replaces a stale file or
/// a link rather than following it. A folder it can't write to leaves no file, said once on the error stream; the
/// server listens all the same. Main thread only.
class EditorPort {
public:
	/// @brief Says on the standard output where the server listens, as the clients' own servers do, and writes the
	/// file.
	/// @param folder The app's folder (its entry file's).
	/// @param port The port listened on, on 127.0.0.1.
	EditorPort(const std::filesystem::path& folder, std::uint16_t port)
	    : file_(folder / ".nrepl-port"), digits_(std::to_string(port)) {
		std::cout << "nREPL server started on port " << digits_ << " on host 127.0.0.1 - nrepl://127.0.0.1:" << digits_
		          << std::endl; // flushed: a script waiting on the line reads it at once
		if(!write()) {
			std::cerr << "nREPL: could not write " << file_.string() << ": give your nREPL client the port " << digits_
			          << '\n';
			file_.clear();
		}
	}

	/// @brief Removes the file, if it still holds this port and is a file of its own (not a link put there since).
	~EditorPort() {
		if(file_.empty()) return;
		std::error_code error;
		if(!std::filesystem::is_regular_file(std::filesystem::symlink_status(file_, error))) return;
		if(read(file_) == digits_) std::filesystem::remove(file_, error);
	}

	EditorPort(const EditorPort&) = delete;
	EditorPort& operator=(const EditorPort&) = delete;
	EditorPort(EditorPort&&) = delete;
	EditorPort& operator=(EditorPort&&) = delete;

private:
	/// @brief Writes the port into a new file beside the name, then renames it over the name.
	/// @return Whether the file holds the port.
	[[nodiscard]] bool write() const {
		std::error_code error;
		const std::filesystem::file_status status = std::filesystem::symlink_status(file_, error);
		// A folder, or anything but a file or a link, under that name is the person's: left alone
		if(std::filesystem::exists(status) && !std::filesystem::is_regular_file(status) &&
		   !std::filesystem::is_symlink(status))
			return false;
		std::filesystem::path written = file_;
		written += ".writing";
		std::filesystem::remove(written, error); // a link there is removed, never followed
		{
			// noreplace: if anything appeared under that name since, opening fails instead of writing through it
			std::ofstream out(written, std::ios::binary | std::ios::out | std::ios::noreplace);
			if(!(out << digits_)) {
				out.close();
				std::filesystem::remove(written, error);
				return false;
			}
		}
		std::filesystem::rename(written, file_, error);
		if(!error) return true;
		std::filesystem::remove(written, error);
		return false;
	}

	/// @brief The start of a small file.
	/// @param file The file.
	/// @return Its first 32 bytes, or "" if it can't be read.
	[[nodiscard]] static std::string read(const std::filesystem::path& file) {
		std::ifstream in(file, std::ios::binary);
		std::string text(32, '\0');
		in.read(text.data(), static_cast<std::streamsize>(text.size()));
		text.resize(static_cast<std::size_t>(in.gcount()));
		return text;
	}

	std::filesystem::path file_; ///< `.nrepl-port` in the folder; empty when it couldn't be written.
	std::string digits_;         ///< The port, in digits.
};
