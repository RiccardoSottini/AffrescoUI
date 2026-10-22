/// @file
/// @brief Modello's file natives (ModelloInternal.hpp): listing the glTF files it may import, importing one into the
/// scene, loading the meshes of the document's imports at start, and exporting the scene.
///
/// Files are read only from two folders, the bundled samples (`samples`) and the data folder's `imports/`
/// (`imports`), by relative paths without `..` that must stay inside them once links are resolved, and only `.gltf`
/// and `.glb`. Exports are written only to the data folder's `exports/`, under a sanitised name. Paths outside these
/// folders never reach a document or a log.

#include "ModelloInternal.hpp"

#include <affresco/scene3d/Gltf.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <algorithm>
#include <fstream>
#include <set>

namespace modello::detail {

namespace {

using namespace affresco::scene3d;
namespace fs = std::filesystem;

/// @brief The most files listed from a folder.
constexpr std::size_t maxListed = 200;
/// @brief The longest export name.
constexpr std::size_t maxExportName = 64;

/// @brief A folder files are imported from. @param model The folders. @param source "samples" or "imports".
/// @return It. @throws std::runtime_error For any other source.
fs::path sourceFolder(const Model& model, const std::string& source) {
	if(source == "samples") return model.samples;
	if(source == "imports") return model.data / "imports";
	throw std::runtime_error("unknown source '" + source + "'");
}

/// @brief Whether a path names a glTF file by its extension. @param path It. @return true for `.gltf` and `.glb`.
bool isGltf(const fs::path& path) {
	std::string extension = path.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return extension == ".gltf" || extension == ".glb";
}

/// @brief A file to import, checked: a relative path without `..`, a glTF file, inside its folder once links are
/// resolved. @param model The folders. @param source "samples" or "imports". @param file The relative path.
/// @return Its full path. @throws std::runtime_error If it may not be read.
fs::path importable(const Model& model, const std::string& source, const std::string& file) {
	const fs::path folder = sourceFolder(model, source);
	if(!isSafeRelativeUri(file) || !isGltf(file)) throw std::runtime_error("'" + file + "' may not be imported");
	std::error_code error;
	const fs::path root = fs::canonical(folder, error);
	const fs::path full = error ? fs::path() : fs::canonical(folder / file, error);
	if(error || !fs::is_regular_file(full, error)) throw std::runtime_error("'" + file + "' was not found");
	const fs::path inside = full.lexically_relative(root);
	if(inside.empty() || *inside.begin() == "..") throw std::runtime_error("'" + file + "' may not be imported");
	return full;
}

/// @brief The glTF files of a folder, two levels deep, without following links. @param folder It.
/// @param source Its name. @param files Receives `{ source, file, name }`.
void listFolder(const fs::path& folder, const std::string& source, json& files) {
	std::error_code error;
	fs::recursive_directory_iterator walk(folder, fs::directory_options::skip_permission_denied, error);
	for(; !error && walk != fs::recursive_directory_iterator() && files.size() < maxListed; walk.increment(error)) {
		if(walk.depth() > 1) walk.disable_recursion_pending();
		if(walk->is_symlink() || !walk->is_regular_file() || !isGltf(walk->path())) continue;
		const std::string relative = walk->path().lexically_relative(folder).generic_string();
		files.push_back({{"source", source}, {"file", relative}, {"name", walk->path().stem().string()}});
	}
}

/// @brief The files that may be imported. @param model The folders. @return `{ files }`, sorted by name.
json listFiles(const Model& model) {
	json files = json::array();
	listFolder(model.samples, "samples", files);
	listFolder(model.data / "imports", "imports", files);
	// NOLINTNEXTLINE(modernize-use-ranges): a json array's iterators aren't a range std::ranges::sort takes
	std::sort(files.begin(), files.end(), [](const json& a, const json& b) {
		return a["name"].get<std::string>() < b["name"].get<std::string>();
	});
	return {{"files", std::move(files)}};
}

/// @brief A prefix for an import's ids that no id in the scene, and no earlier import, starts with.
/// @param scene The scene. @param imports The document's imports. @return E.g. "import2_".
std::string freshPrefix(const Scene& scene, const json& imports) {
	std::set<std::string, std::less<>> used;
	if(imports.is_array())
		for(const json& entry : imports) used.insert(textIn(entry, "prefix"));
	const auto taken = [&](const std::string& prefix) {
		const auto starts = [&](const std::string& id) { return id.starts_with(prefix); };
		return used.contains(prefix) || std::ranges::any_of(scene.nodes, starts, &Node::id) ||
		       std::ranges::any_of(scene.materials, starts, &Material::id);
	};
	std::string prefix;
	for(std::size_t n = 1; prefix.empty() || taken(prefix); ++n) prefix = "import" + std::to_string(n) + "_";
	return prefix;
}

/// @brief Loads a file's meshes and textures into the store. @param model The store. @param entry `{ source, file,
/// prefix }`. @return What it held.
GltfImport loadInto(Model& model, const json& entry) { // NOLINT(misc-const-correctness): it fills the store
	GltfImport loaded = loadGltfFile(importable(model, textIn(entry, "source"), textIn(entry, "file")), GltfLimits{},
	                                 textIn(entry, "prefix"));
	model.assets->merge(loaded.assets);
	return loaded;
}

/// @brief Imports a file into the scene: its nodes, materials and animations are appended and it is recorded in
/// `computed.imports`, so that its meshes load again with the document. @param model The store.
/// @param with The arguments (`source`, `file`, `scene`, `animations`, `imports`).
/// @return `{ patch, selection, warnings }`.
json importFile(Model& model, const json& with) {
	const json sceneJson = with.value("scene", json::object());
	const json imports = with.value("imports", json());
	json entry = {{"source", textIn(with, "source")},
	              {"file", textIn(with, "file")},
	              {"prefix", freshPrefix(sceneFromJson(sceneJson), imports)}};
	const GltfImport loaded = loadInto(model, entry);
	json nodes = json::array(), materials = json::array(), selection = json::array();
	for(const Node& node : loaded.scene.nodes) {
		nodes.push_back(toJson(node));
		if(node.parent.empty()) selection.push_back(node.id);
	}
	for(const Material& material : loaded.scene.materials) materials.push_back(toJson(material));
	json patch = json::array();
	appendTo(patch, sceneJson.value("materials", json()), std::string(scenePath) + "/materials", materials);
	appendTo(patch, sceneJson.value("nodes", json()), std::string(scenePath) + "/nodes", nodes);
	appendTo(patch, with.value("animations", json()), std::string(animationsPath), toJson(loaded.animations));
	appendTo(patch, imports, "/computed/imports", json::array({std::move(entry)}));
	return {{"patch", std::move(patch)}, {"selection", std::move(selection)}, {"warnings", loaded.warnings}};
}

/// @brief Loads the meshes of the document's imports, at start. @param model The store.
/// @param with The arguments (`imports`). @return `{ loaded, warnings }`: what failed is a warning.
json loadImports(Model& model, const json& with) {
	json warnings = json::array();
	std::size_t count = 0;
	const json imports = with.value("imports", json::array());
	for(const json& entry : imports.is_array() ? imports : json::array()) {
		try {
			for(std::string& warning : loadInto(model, entry).warnings) warnings.push_back(std::move(warning));
			++count;
		} catch(const std::exception& error) {
			warnings.push_back(textIn(entry, "file") + ": " + error.what());
		}
	}
	return {{"loaded", count}, {"warnings", std::move(warnings)}};
}

/// @brief A file name made of letters, digits, `-` and `_` only. @param name What was asked for.
/// @return It, sanitised and shortened, or "scene".
std::string exportName(const std::string& name) {
	std::string clean;
	for(const char c : name)
		if(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') clean.push_back(c);
	clean.resize(std::min(clean.size(), maxExportName));
	return clean.empty() ? "scene" : clean;
}

/// @brief Writes the scene as a `.gltf` file to the data folder's `exports/`. @param model The store and folders.
/// @param with The arguments (`scene`, `animations`, `name`). @return `{ file, warnings }`, the file relative to the
/// data folder.
json exportScene(const Model& model, const json& with) {
	const Scene scene = sceneFromJson(with.value("scene", json::object()));
	const json animations = with.value("animations", json());
	const auto clips = animations.is_array() ? clipsFromJson(animations) : std::vector<AnimationClip>{};
	std::vector<std::string> warnings;
	const std::string text = exportGltf(scene, *model.assets, clips, &warnings);
	const fs::path folder = model.data / "exports";
	fs::create_directories(folder);
	const std::string file = exportName(textIn(with, "name")) + ".gltf";
	std::ofstream out(folder / file, std::ios::binary | std::ios::trunc);
	out << text;
	if(!out.good()) throw std::runtime_error("could not write exports/" + file);
	return {{"file", "exports/" + file}, {"warnings", warnings}};
}

} // namespace

void registerFileNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model) {
	define(screen, "modello.samples", [model](const json&) { return listFiles(*model); });
	define(screen, "modello.import", [model](const json& with) { return importFile(*model, with); });
	define(screen, "modello.load", [model](const json& with) { return loadImports(*model, with); });
	define(screen, "modello.export", [model](const json& with) { return exportScene(*model, with); });
}

} // namespace modello::detail
