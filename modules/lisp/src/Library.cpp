/// @file
/// @brief The library table (Library.hpp): entries registered at the start, read and compiled on first use, against
/// the language's own cells, which no program writes.

#include "Library.hpp"

#include "Compiler.hpp"
#include "Machine.hpp"
#include "Reader.hpp"

#include <affresco/notation/Lexer.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace affresco::lisp {

namespace {

/// @brief Puts the budget aside while the language compiles its own code, and the depth of the compile around it:
/// what the language costs to load isn't a program's, wherever its first use falls.
class LanguageWork {
public:
	/// @brief Starts. @param machine The machine.
	explicit LanguageWork(Machine& machine) noexcept
	    : machine_(machine), budget_(machine.budget()), depth_(std::exchange(machine.compileDepth(), 0)),
	      gensym_(std::exchange(machine.gensymCounter(), 0)) {
		machine_.budget() = {static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)};
	}
	~LanguageWork() {
		machine_.budget() = budget_;
		machine_.compileDepth() = depth_;
		machine_.gensymCounter() = gensym_;
	}
	LanguageWork(const LanguageWork&) = delete;
	LanguageWork& operator=(const LanguageWork&) = delete;

private:
	Machine& machine_;     ///< See the constructor.
	Budget budget_;        ///< What it was.
	int depth_;            ///< What it was.
	std::uint64_t gensym_; ///< What it was: a macro's gensyms are numbered per definition.
};

/// @brief Reads and compiles an entry into the language's own cell.
/// @param machine The machine.
/// @param own The cell.
/// @param entry The entry.
/// @throws std::logic_error If its source isn't one definition of its name: the table is wrong.
void compileEntry(Machine& machine, Global& own, const LibraryEntry& entry) {
	const LanguageWork work(machine);
	Reader reader(machine.heap, entry.source, "<library>");
	const std::optional<ReadForm> read = reader.next();
	const std::string_view head = read ? headWord(read->form) : std::string_view();
	if(!read || head != (entry.macro ? "defmacro" : "defn") || read->form.count() < 3 ||
	   read->form.at(1).text() != entry.name || reader.next())
		throw std::logic_error("the library's entry \"" + std::string(entry.name) + "\" isn't one " +
		                       (entry.macro ? "defmacro" : "defn") + " of that name");
	CompileContext context;
	context.definition = std::string(entry.name);
	context.library = true;
	auto* closure = machine.heap.make<ClosureObject>();
	closure->code = Compiler(machine, std::move(context)).function(read->form);
	own.value = Value::object(Kind::Function, closure);
	own.state = Global::State::Ready;
}

/// @brief Marks a cell as an entry's, waiting to be compiled.
/// @param cell The cell.
/// @param entry The entry.
void markEntry(Global& cell, const LibraryEntry& entry) {
	cell.state = Global::State::Deferred;
	cell.library = true;
	cell.macro = entry.macro;
	cell.takesFunctions = entry.takesFunctions;
	cell.uses = entry.uses;
}

} // namespace

void defineLibrary(Machine& machine, std::span<const LibraryEntry> entries) {
	for(const LibraryEntry& entry : entries) {
		Global& own = machine.languageGlobal(entry.name);
		markEntry(own, entry);
		own.pending = [&machine, &own, &entry] { compileEntry(machine, own, entry); };
		// The global a program sees: the same function, until a definition of the program replaces it
		Global& cell = machine.global(entry.name);
		markEntry(cell, entry);
		cell.pending = [&own, &cell] {
			Machine::ensureCompiled(own);
			cell.value = own.value;
			cell.state = Global::State::Ready;
		};
	}
}

std::string definedUse(const Machine& machine, const Global& macro) {
	std::string problem;
	forEachUse(macro.uses, [&](std::string_view word) {
		const Global* cell = machine.findGlobal(word); // a definition read, or one already run
		if(problem.empty() && (machine.definition(word) || (cell && !cell->library)))
			problem = "\"" + macro.name + "\" uses the language's \"" + std::string(word) +
			          "\", which this program defines: name yours otherwise";
	});
	return problem;
}

} // namespace affresco::lisp
