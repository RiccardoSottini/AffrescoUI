/// @file
/// @brief Arranging a slide's objects: align, distribute, z-order, group and ungroup, and nudge.

#include "Arrange.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace sinopia {

namespace {

/// @brief Whether an object is one of the ids and may be moved.
/// @param object The object.
/// @param ids The ids.
/// @return Whether.
[[nodiscard]] bool chosen(const SlideObject& object, std::span<const std::string> ids) {
	return !object.locked && std::ranges::contains(ids, object.id);
}

/// @brief The objects of a slide that are chosen and not locked.
/// @param objects The slide's objects.
/// @param ids The ids.
/// @return Pointers to them, back to front.
[[nodiscard]] std::vector<SlideObject*> movable(std::vector<SlideObject>& objects, std::span<const std::string> ids) {
	std::vector<SlideObject*> found;
	for(SlideObject& object : objects)
		if(chosen(object, ids)) found.push_back(&object);
	return found;
}

/// @brief A box: left, top, right, bottom.
struct Box {
	float left = std::numeric_limits<float>::max();      ///< Left.
	float top = std::numeric_limits<float>::max();       ///< Top.
	float right = std::numeric_limits<float>::lowest();  ///< Right.
	float bottom = std::numeric_limits<float>::lowest(); ///< Bottom.

	/// @brief Grows to hold an object.
	/// @param object The object.
	void add(const SlideObject& object) noexcept {
		left = std::min(left, object.x);
		top = std::min(top, object.y);
		right = std::max(right, object.x + object.w);
		bottom = std::max(bottom, object.y + object.h);
	}
};

} // namespace

void alignObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& edge,
                  bool toSlide, std::pair<float, float> slide) {
	constexpr std::array<std::string_view, 6> edges{"left", "center", "right", "top", "middle", "bottom"};
	if(!std::ranges::contains(edges, std::string_view(edge))) throw std::runtime_error("Sinopia: no edge " + edge);
	const std::vector<SlideObject*> moving = movable(objects, ids);
	if(moving.empty()) return;
	Box box;
	if(toSlide || moving.size() == 1) box = {0.0f, 0.0f, slide.first, slide.second};
	else
		for(const SlideObject* object : moving) box.add(*object);
	for(SlideObject* object : moving) {
		if(edge == "left") object->x = box.left;
		else if(edge == "center") object->x = (box.left + box.right - object->w) / 2.0f;
		else if(edge == "right") object->x = box.right - object->w;
		else if(edge == "top") object->y = box.top;
		else if(edge == "middle") object->y = (box.top + box.bottom - object->h) / 2.0f;
		else object->y = box.bottom - object->h;
	}
}

void distributeObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, bool across) {
	std::vector<SlideObject*> moving = movable(objects, ids);
	if(moving.size() < 3) return;
	const auto start = [across](const SlideObject* object) { return across ? object->x : object->y; };
	const auto length = [across](const SlideObject* object) { return across ? object->w : object->h; };
	std::ranges::stable_sort(moving, {}, start);
	float used = 0.0f;
	for(const SlideObject* object : moving) used += length(object);
	const float first = start(moving.front());
	const float last = start(moving.back()) + length(moving.back());
	const float gap = (last - first - used) / static_cast<float>(moving.size() - 1);
	float at = first;
	for(SlideObject* object : moving) {
		(across ? object->x : object->y) = at;
		at += length(object) + gap;
	}
}

void reorderObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& how) {
	const auto isChosen = [ids](const SlideObject& object) { return std::ranges::contains(ids, object.id); };
	if(how == "front") std::ranges::stable_partition(objects, [&](const SlideObject& o) { return !isChosen(o); });
	else if(how == "back") std::ranges::stable_partition(objects, isChosen);
	else if(how == "forward") {
		// From the front, each chosen object swaps with the one in front of it, unless that one is chosen too
		for(std::size_t i = objects.size(); i-- > 1;)
			if(isChosen(objects[i - 1]) && !isChosen(objects[i])) std::swap(objects[i - 1], objects[i]);
	} else if(how == "backward") {
		for(std::size_t i = 1; i < objects.size(); ++i)
			if(isChosen(objects[i]) && !isChosen(objects[i - 1])) std::swap(objects[i - 1], objects[i]);
	} else throw std::runtime_error("Sinopia: no z-order " + how);
}

void nudgeObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, float x, float y) {
	for(SlideObject* object : movable(objects, ids)) {
		object->x += x;
		object->y += y;
	}
}

bool groupObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids, const std::string& id) {
	const std::vector<SlideObject*> moving = movable(objects, ids);
	if(moving.size() < 2 || std::ranges::any_of(moving, [](const SlideObject* o) { return o->kind == "group"; }))
		return false;
	Box box;
	for(const SlideObject* object : moving) box.add(*object);
	SlideObject group;
	group.id = id;
	group.kind = "group";
	group.x = box.left;
	group.y = box.top;
	group.w = box.right - box.left;
	group.h = box.bottom - box.top;
	std::size_t place = 0;
	for(std::size_t i = 0; i < objects.size(); ++i)
		if(chosen(objects[i], ids)) place = i;
	place -= static_cast<std::size_t>(std::ranges::count_if(objects.begin(),
	                                                        objects.begin() + static_cast<std::ptrdiff_t>(place),
	                                                        [ids](const SlideObject& o) { return chosen(o, ids); }));
	for(const SlideObject& object : objects)
		if(chosen(object, ids)) {
			SlideObject child = object;
			child.x -= box.left;
			child.y -= box.top;
			group.children.push_back(std::move(child));
		}
	std::erase_if(objects, [ids](const SlideObject& o) { return chosen(o, ids); });
	objects.insert(objects.begin() + static_cast<std::ptrdiff_t>(place), std::move(group));
	return true;
}

void ungroupObjects(std::vector<SlideObject>& objects, std::span<const std::string> ids) {
	std::vector<SlideObject> result;
	for(SlideObject& object : objects) {
		if(object.kind != "group" || !chosen(object, ids)) {
			result.push_back(std::move(object));
			continue;
		}
		for(SlideObject& child : object.children) {
			child.x += object.x;
			child.y += object.y;
			result.push_back(std::move(child));
		}
	}
	objects = std::move(result);
}

} // namespace sinopia
