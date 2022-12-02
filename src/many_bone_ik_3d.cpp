/*************************************************************************/
/*  ik_many_bone_ik.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "many_bone_ik_3d.h"
#include "core/core_string_names.h"
#include "core/error/error_macros.h"
#include "ik_bone_3d.h"
#include "ik_kusudama.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif

void ManyBoneIK3D::set_pin_count(int32_t p_value) {
	int32_t old_count = pins.size();
	pin_count = p_value;
	pins.resize(p_value);
	for (int32_t pin_i = p_value; pin_i-- > old_count;) {
		pins.write[pin_i].instantiate();
	}
	set_dirty();
	notify_property_list_changed();
}

int32_t ManyBoneIK3D::get_pin_count() const {
	return pin_count;
}

void ManyBoneIK3D::set_pin_bone(int32_t p_pin_index, const String &p_bone) {
	ERR_FAIL_INDEX(p_pin_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	if (effector_template.is_null()) {
		effector_template.instantiate();
		pins.write[p_pin_index] = effector_template;
	}
	effector_template->set_name(p_bone);
	set_dirty();
}

void ManyBoneIK3D::set_pin_target_nodepath(int32_t p_pin_index, const NodePath &p_target_node) {
	ERR_FAIL_INDEX(p_pin_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	if (effector_template.is_null()) {
		effector_template.instantiate();
		pins.write[p_pin_index] = effector_template;
	}
	effector_template->set_target_node(p_target_node);
	set_dirty();
}

NodePath ManyBoneIK3D::get_pin_target_nodepath(int32_t p_pin_index) {
	ERR_FAIL_INDEX_V(p_pin_index, pins.size(), NodePath());
	const Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	return effector_template->get_target_node();
}

Vector<Ref<IKEffectorTemplate>> ManyBoneIK3D::get_bone_effectors() const {
	return pins;
}

void ManyBoneIK3D::remove_pin(int32_t p_index) {
	ERR_FAIL_INDEX(p_index, pins.size());
	pins.remove_at(p_index);
	pin_count--;
	pins.resize(pin_count);
	set_dirty();
	notify_property_list_changed();
}

void ManyBoneIK3D::update_ik_bones_transform() {
	for (int32_t bone_i = bone_list.size(); bone_i-- > 0;) {
		Ref<IKBone3D> bone = bone_list[bone_i];
		if (bone.is_null()) {
			continue;
		}
		bone->set_initial_pose(get_skeleton());
		if (bone->is_pinned()) {
			bone->get_pin()->update_target_global_transform(get_skeleton(), this);
		}
	}
}

void ManyBoneIK3D::update_skeleton_bones_transform() {
	for (int32_t bone_i = bone_list.size(); bone_i-- > 0;) {
		Ref<IKBone3D> bone = bone_list[bone_i];
		if (bone.is_null()) {
			continue;
		}
		if (bone->get_bone_id() == -1) {
			continue;
		}
		bone->set_skeleton_bone_pose(get_skeleton());
	}
}

void ManyBoneIK3D::_get_property_list(List<PropertyInfo> *p_list) const {
	RBSet<String> existing_constraints;
	for (int32_t constraint_i = 0; constraint_i < get_constraint_count(); constraint_i++) {
		const String name = get_constraint_name(constraint_i);
		existing_constraints.insert(name);
	}
	RBSet<StringName> existing_bones;
	for (int32_t bone_i = 0; bone_i < get_bone_count(); bone_i++) {
		const StringName name = get_bone_damp_bone_name(bone_i);
		existing_bones.insert(name);
	}
	{
		const uint32_t damp_usage = get_edit_constraint_mode() == NBONE_IK_EDIT_CONSTRAIN_MODE_DAMP ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_STORAGE;
		p_list->push_back(
				PropertyInfo(Variant::INT, "bone_count",
						PROPERTY_HINT_RANGE, "0,256,or_greater", damp_usage | PROPERTY_USAGE_ARRAY,
						"Bone,bone/"));
		for (int bone_i = 0; bone_i < get_bone_count(); bone_i++) {
			PropertyInfo bone_name;
			bone_name.type = Variant::STRING_NAME;
			bone_name.usage = damp_usage;
			bone_name.name = "bone/" + itos(bone_i) + "/bone_name";
			if (get_skeleton()) {
				String names;
				Vector<BoneId> root_bones = get_skeleton()->get_parentless_bones();
				if (!get_edit_constraint_mode()) {
					for (int bone_i = 0; bone_i < get_skeleton()->get_bone_count(); bone_i++) {
						String name = get_skeleton()->get_bone_name(bone_i);
						if (existing_bones.has(name)) {
							names = name;
							break;
						}
					}
				} else {
					for (int bone_i = 0; bone_i < get_skeleton()->get_bone_count(); bone_i++) {
						String name = get_skeleton()->get_bone_name(bone_i);
						if (root_bones.find(bone_i) != -1) {
							continue;
						}
						name += ",";
						names += name;
						existing_bones.insert(name);
					}
				}
				bone_name.hint = PROPERTY_HINT_ENUM_SUGGESTION;
				bone_name.hint_string = names;
			} else {
				bone_name.hint = PROPERTY_HINT_NONE;
				bone_name.hint_string = "";
			}
			p_list->push_back(bone_name);
			p_list->push_back(
					PropertyInfo(Variant::FLOAT, "bone/" + itos(bone_i) + "/damp", PROPERTY_HINT_RANGE, "0,360,0.01,radians", damp_usage));
		}
	}
	const uint32_t constraint_usage = get_edit_constraint_mode() == NBONE_IK_EDIT_CONSTRAIN_MODE_LOCK || get_edit_constraint_mode() == NBONE_IK_EDIT_CONSTRAIN_MODE_AUTOMATIC_UNLOCK ? PROPERTY_USAGE_DEFAULT : PROPERTY_USAGE_STORAGE;
	p_list->push_back(
			PropertyInfo(Variant::INT, "constraint_count",
					PROPERTY_HINT_RANGE, "0,256,or_greater", constraint_usage | PROPERTY_USAGE_ARRAY,
					"Kusudama Constraints,constraints/"));
	for (int constraint_i = 0; constraint_i < get_constraint_count(); constraint_i++) {
		PropertyInfo bone_name;
		bone_name.type = Variant::STRING_NAME;
		bone_name.usage = constraint_usage;
		bone_name.name = "constraints/" + itos(constraint_i) + "/bone_name";
		if (get_skeleton()) {
			String names;
			Vector<BoneId> root_bones = get_skeleton()->get_parentless_bones();
			if (get_edit_constraint_mode() != NBONE_IK_EDIT_CONSTRAIN_MODE_OFF) {
				for (int bone_i = 0; bone_i < get_skeleton()->get_bone_count(); bone_i++) {
					String name = get_skeleton()->get_bone_name(bone_i);
					if (existing_constraints.has(name)) {
						names = name;
						break;
					}
				}
			} else {
				for (int bone_i = 0; bone_i < get_skeleton()->get_bone_count(); bone_i++) {
					String name = get_skeleton()->get_bone_name(bone_i);
					if (existing_constraints.has(name)) {
						continue;
					}
					if (root_bones.find(bone_i) != -1) {
						continue;
					}
					name += ",";
					names += name;
					existing_constraints.insert(name);
				}
			}
			bone_name.hint = PROPERTY_HINT_ENUM_SUGGESTION;
			bone_name.hint_string = names;
		} else {
			bone_name.hint = PROPERTY_HINT_NONE;
			bone_name.hint_string = "";
		}
		p_list->push_back(bone_name);
		p_list->push_back(
				PropertyInfo(Variant::FLOAT, "constraints/" + itos(constraint_i) + "/twist_from", PROPERTY_HINT_RANGE, "0,360,0.1,radians,or_lesser,or_greater", constraint_usage));
		p_list->push_back(
				PropertyInfo(Variant::FLOAT, "constraints/" + itos(constraint_i) + "/twist_range", PROPERTY_HINT_RANGE, "0,360,0.1,radians", constraint_usage));
		p_list->push_back(
				PropertyInfo(Variant::FLOAT, "constraints/" + itos(constraint_i) + "/twist_current", PROPERTY_HINT_RANGE, "0,1,0.001", constraint_usage));
		p_list->push_back(
				PropertyInfo(Variant::INT, "constraints/" + itos(constraint_i) + "/kusudama_limit_cone_count",
						PROPERTY_HINT_RANGE, "0,30,1", constraint_usage | PROPERTY_USAGE_ARRAY,
						"Limit Cones,constraints/" + itos(constraint_i) + "/kusudama_limit_cone/"));
		for (int cone_i = 0; cone_i < get_kusudama_limit_cone_count(constraint_i); cone_i++) {
			p_list->push_back(
					PropertyInfo(Variant::VECTOR3, "constraints/" + itos(constraint_i) + "/kusudama_limit_cone/" + itos(cone_i) + "/center", PROPERTY_HINT_RANGE, "-1.0,1.0,0.01,or_greater", constraint_usage));
			p_list->push_back(
					PropertyInfo(Variant::FLOAT, "constraints/" + itos(constraint_i) + "/kusudama_limit_cone/" + itos(cone_i) + "/radius", PROPERTY_HINT_RANGE, "0,180,0.1,radians", constraint_usage));
		}
	}
	RBSet<String> existing_pins;
	for (int32_t pin_i = 0; pin_i < get_pin_count(); pin_i++) {
		const String name = get_pin_bone_name(pin_i);
		existing_pins.insert(name);
	}
	const uint32_t pin_usage = get_edit_constraint_mode() != NBONE_IK_EDIT_CONSTRAIN_MODE_OFF ? PROPERTY_USAGE_STORAGE : PROPERTY_USAGE_DEFAULT;
	p_list->push_back(
			PropertyInfo(Variant::INT, "pin_count",
					PROPERTY_HINT_RANGE, "0,1024,or_greater", pin_usage | PROPERTY_USAGE_ARRAY,
					"Pins,pins/"));
	for (int pin_i = 0; pin_i < pin_count; pin_i++) {
		PropertyInfo effector_name;
		effector_name.type = Variant::STRING_NAME;
		effector_name.name = "pins/" + itos(pin_i) + "/bone_name";
		effector_name.usage = pin_usage;
		if (get_skeleton()) {
			String names;
			for (int bone_i = 0; bone_i < get_skeleton()->get_bone_count(); bone_i++) {
				String name = get_skeleton()->get_bone_name(bone_i);
				if (existing_pins.has(name)) {
					continue;
				}
				name += ",";
				names += name;
			}
			effector_name.hint = PROPERTY_HINT_ENUM_SUGGESTION;
			effector_name.hint_string = names;
		} else {
			effector_name.hint = PROPERTY_HINT_NONE;
			effector_name.hint_string = "";
		}
		p_list->push_back(effector_name);
		p_list->push_back(
				PropertyInfo(Variant::NODE_PATH, "pins/" + itos(pin_i) + "/target_node", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D", pin_usage));
		p_list->push_back(
				PropertyInfo(Variant::FLOAT, "pins/" + itos(pin_i) + "/passthrough_factor", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater", pin_usage));
		p_list->push_back(
				PropertyInfo(Variant::FLOAT, "pins/" + itos(pin_i) + "/weight", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater", pin_usage));
		p_list->push_back(
				PropertyInfo(Variant::VECTOR3, "pins/" + itos(pin_i) + "/direction_priorities", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater", pin_usage));
	}
}

bool ManyBoneIK3D::_get(const StringName &p_name, Variant &r_ret) const {
	String name = p_name;
	if (name == "constraint_count") {
		r_ret = get_constraint_count();
		return true;
	} else if (name == "pin_count") {
		r_ret = get_pin_count();
		return true;
	} else if (name == "bone_count") {
		r_ret = get_bone_count();
		return true;
	} else if (name.begins_with("pins/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(index, pins.size(), false);
		Ref<IKEffectorTemplate> effector_template = pins[index];
		ERR_FAIL_NULL_V(effector_template, false);
		if (what == "bone_name") {
			r_ret = effector_template->get_name();
			return true;
		} else if (what == "target_node") {
			r_ret = effector_template->get_target_node();
			return true;
		} else if (what == "passthrough_factor") {
			r_ret = get_pin_passthrough_factor(index);
			return true;
		} else if (what == "weight") {
			r_ret = get_pin_weight(index);
			return true;
		} else if (what == "direction_priorities") {
			r_ret = get_pin_direction_priorities(index);
			return true;
		}
	} else if (name.begins_with("bone/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(index, bone_count, false);
		if (what == "bone_name") {
			r_ret = get_bone_damp_bone_name(index);
			return true;
		} else if (what == "damp") {
			r_ret = get_bone_damp(index);
			return true;
		}
	} else if (name.begins_with("constraints/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(index, constraint_count, false);
		String begins = "constraints/" + itos(index) + "/kusudama_limit_cone";
		if (what == "bone_name") {
			ERR_FAIL_INDEX_V(index, constraint_names.size(), false);
			r_ret = constraint_names[index];
			return true;
		} else if (what == "twist_current") {
			String bone_name = constraint_names[index];
			for (Ref<IKBoneSegment> segmented_skeleton : segmented_skeletons) {
				if (segmented_skeleton.is_null()) {
					r_ret = 0;
					continue;
				}
				if (!get_skeleton()) {
					r_ret = 0;
					continue;
				}
				Ref<IKBone3D> ik_bone = segmented_skeleton->get_ik_bone(get_skeleton()->find_bone(bone_name));
				if (ik_bone.is_null()) {
					r_ret = 0;
					continue;
				}
				if (ik_bone->get_constraint().is_null()) {
					r_ret = 0;
					continue;
				}
				r_ret = ik_bone->get_constraint()->get_current_twist_rotation(ik_bone);
				return true;
			}
			r_ret = 0;
			return false;
		} else if (what == "twist_from") {
			r_ret = get_kusudama_twist(index).x;
			return true;
		} else if (what == "twist_range") {
			r_ret = get_kusudama_twist(index).y;
			return true;
		} else if (what == "kusudama_limit_cone_count") {
			r_ret = get_kusudama_limit_cone_count(index);
			return true;
		} else if (name.begins_with(begins)) {
			int32_t cone_index = name.get_slicec('/', 3).to_int();
			String cone_what = name.get_slicec('/', 4);
			if (cone_what == "center") {
				r_ret = get_kusudama_limit_cone_center(index, cone_index);
				return true;
			} else if (cone_what == "radius") {
				r_ret = get_kusudama_limit_cone_radius(index, cone_index);
				return true;
			}
		}
	}
	return false;
}

bool ManyBoneIK3D::_set(const StringName &p_name, const Variant &p_value) {
	String name = p_name;
	if (name == "constraint_count") {
		set_constraint_count(p_value);
		return true;
	} else if (name == "pin_count") {
		set_pin_count(p_value);
		return true;
	} else if (name == "bone_count") {
		set_bone_count(p_value);
		return true;
	} else if (name.begins_with("pins/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		ERR_FAIL_INDEX_V(index, pin_count, true);
		if (what == "bone_name") {
			set_pin_bone(index, p_value);
			return true;
		} else if (what == "target_node") {
			set_pin_target_nodepath(index, p_value);
			String existing_bone = get_pin_bone_name(index);
			if (existing_bone.is_empty()) {
				return false;
			}
			return true;
		} else if (what == "passthrough_factor") {
			set_pin_passthrough_factor(index, p_value);
			return true;
		} else if (what == "weight") {
			set_pin_weight(index, p_value);
			return true;
		} else if (what == "direction_priorities") {
			set_pin_direction_priorities(index, p_value);
			return true;
		}
	} else if (name.begins_with("bone/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		if (what == "bone_name") {
			if (index >= bone_damp.size()) {
				set_bone_count(bone_count);
			}
			set_bone_damp_bone_name(index, p_value);
			return true;
		} else if (what == "damp") {
			set_bone_damp(index, p_value);
			return true;
		}
	} else if (name.begins_with("constraints/")) {
		int index = name.get_slicec('/', 1).to_int();
		String what = name.get_slicec('/', 2);
		String begins = "constraints/" + itos(index) + "/kusudama_limit_cone/";
		if (what == "bone_name") {
			if (index >= constraint_names.size()) {
				set_constraint_count(constraint_count);
			}
			set_constraint_name(index, p_value);
			return true;
		} else if (what == "twist_current") {
			Vector2 twist_from = get_kusudama_twist(index);
			for (Ref<IKBoneSegment> segmented_skeleton : segmented_skeletons) {
				if (segmented_skeleton.is_null()) {
					continue;
				}
				if (!get_skeleton()) {
					continue;
				}
				String bone_name = constraint_names[index];
				Ref<IKBone3D> ik_bone = segmented_skeleton->get_ik_bone(get_skeleton()->find_bone(bone_name));
				if (ik_bone.is_null()) {
					continue;
				}
				if (ik_bone->get_constraint().is_null()) {
					continue;
				}
				ik_bone->get_constraint()->set_current_twist_rotation(ik_bone, p_value);
				for (int32_t i = 0; i < get_iterations_per_frame(); i++) {
					if (segmented_skeleton.is_null()) {
						break;
					}
					segmented_skeleton->segment_solver(get_default_damp(), constrain_mode);
				}
				update_skeleton_bones_transform();
				return true;
			}
			return false;
		} else if (what == "twist_from") {
			Vector2 twist_from = get_kusudama_twist(index);
			set_kusudama_twist(index, Vector2(IKKusudama::_to_tau(p_value), IKKusudama::_to_tau(twist_from.y)));
			return true;
		} else if (what == "twist_range") {
			Vector2 twist_range = get_kusudama_twist(index);
			set_kusudama_twist(index, Vector2(IKKusudama::_to_tau(twist_range.x), IKKusudama::_to_tau(p_value)));
			return true;
		} else if (what == "kusudama_limit_cone_count") {
			set_kusudama_limit_cone_count(index, p_value);
			return true;
		} else if (name.begins_with(begins)) {
			int cone_index = name.get_slicec('/', 3).to_int();
			String cone_what = name.get_slicec('/', 4);
			if (cone_what == "center") {
				Vector3 center = p_value;
				if (Math::is_zero_approx(center.length_squared())) {
					center = Vector3(0.0, 1.0, 0.0);
				}
				set_kusudama_limit_cone_center(index, cone_index, center);
				return true;
			} else if (cone_what == "radius") {
				set_kusudama_limit_cone_radius(index, cone_index, p_value);
				return true;
			}
		}
	}
	return false;
}

void ManyBoneIK3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_edit_constraint_mode", "enable"), &ManyBoneIK3D::set_edit_constraint_mode);
	ClassDB::bind_method(D_METHOD("get_edit_constraint_mode"), &ManyBoneIK3D::get_edit_constraint_mode);
	ClassDB::bind_method(D_METHOD("get_kusudama_twist_current", "index"), &ManyBoneIK3D::get_kusudama_twist_current);
	ClassDB::bind_method(D_METHOD("set_kusudama_twist_current", "index", "rotation"), &ManyBoneIK3D::set_kusudama_twist_current);
	ClassDB::bind_method(D_METHOD("remove_constraint", "index"), &ManyBoneIK3D::remove_constraint);
	ClassDB::bind_method(D_METHOD("set_skeleton_node_path", "path"), &ManyBoneIK3D::set_skeleton_node_path);
	ClassDB::bind_method(D_METHOD("get_skeleton_node_path"), &ManyBoneIK3D::get_skeleton_node_path);
	ClassDB::bind_method(D_METHOD("set_pin_weight", "index", "weight"), &ManyBoneIK3D::set_pin_weight);
	ClassDB::bind_method(D_METHOD("get_pin_weight", "index"), &ManyBoneIK3D::get_pin_weight);
	ClassDB::bind_method(D_METHOD("set_dirty"), &ManyBoneIK3D::set_dirty);
	ClassDB::bind_method(D_METHOD("set_kusudama_limit_cone_radius", "index", "cone_index", "radius"), &ManyBoneIK3D::set_kusudama_limit_cone_radius);
	ClassDB::bind_method(D_METHOD("get_kusudama_limit_cone_radius", "index", "cone_index"), &ManyBoneIK3D::get_kusudama_limit_cone_radius);
	ClassDB::bind_method(D_METHOD("set_kusudama_limit_cone_center", "index", "cone_index", "center"), &ManyBoneIK3D::set_kusudama_limit_cone_center);
	ClassDB::bind_method(D_METHOD("get_kusudama_limit_cone_center", "index", "cone_index"), &ManyBoneIK3D::get_kusudama_limit_cone_center);
	ClassDB::bind_method(D_METHOD("set_kusudama_limit_cone_count", "index", "count"), &ManyBoneIK3D::set_kusudama_limit_cone_count);
	ClassDB::bind_method(D_METHOD("get_kusudama_limit_cone_count", "index"), &ManyBoneIK3D::get_kusudama_limit_cone_count);
	ClassDB::bind_method(D_METHOD("set_kusudama_twist", "index", "limit"), &ManyBoneIK3D::set_kusudama_twist);
	ClassDB::bind_method(D_METHOD("get_kusudama_twist", "index"), &ManyBoneIK3D::get_kusudama_twist);
	ClassDB::bind_method(D_METHOD("set_pin_passthrough_factor", "index", "falloff"), &ManyBoneIK3D::set_pin_passthrough_factor);
	ClassDB::bind_method(D_METHOD("get_pin_passthrough_factor", "index"), &ManyBoneIK3D::get_pin_passthrough_factor);
	ClassDB::bind_method(D_METHOD("set_constraint_name", "index", "name"), &ManyBoneIK3D::set_constraint_name);
	ClassDB::bind_method(D_METHOD("get_constraint_name", "index"), &ManyBoneIK3D::get_constraint_name);
	ClassDB::bind_method(D_METHOD("get_iterations_per_frame"), &ManyBoneIK3D::get_iterations_per_frame);
	ClassDB::bind_method(D_METHOD("set_iterations_per_frame", "count"), &ManyBoneIK3D::set_iterations_per_frame);
	ClassDB::bind_method(D_METHOD("find_constraint", "name"), &ManyBoneIK3D::find_constraint);
	ClassDB::bind_method(D_METHOD("get_constraint_count"), &ManyBoneIK3D::get_constraint_count);
	ClassDB::bind_method(D_METHOD("set_constraint_count", "count"),
			&ManyBoneIK3D::set_constraint_count);
	ClassDB::bind_method(D_METHOD("get_pin_count"), &ManyBoneIK3D::get_pin_count);
	ClassDB::bind_method(D_METHOD("set_pin_count", "count"),
			&ManyBoneIK3D::set_pin_count);
	ClassDB::bind_method(D_METHOD("remove_pin", "index"),
			&ManyBoneIK3D::remove_pin);
	ClassDB::bind_method(D_METHOD("get_pin_bone_name", "index"), &ManyBoneIK3D::get_pin_bone_name);
	ClassDB::bind_method(D_METHOD("set_pin_bone_name", "index", "name"), &ManyBoneIK3D::set_pin_bone_name);
	ClassDB::bind_method(D_METHOD("get_pin_direction_priorities", "index"), &ManyBoneIK3D::get_pin_direction_priorities);
	ClassDB::bind_method(D_METHOD("set_pin_direction_priorities", "index", "priority"), &ManyBoneIK3D::set_pin_direction_priorities);
	ClassDB::bind_method(D_METHOD("queue_print_skeleton"), &ManyBoneIK3D::queue_print_skeleton);
	ClassDB::bind_method(D_METHOD("get_default_damp"), &ManyBoneIK3D::get_default_damp);
	ClassDB::bind_method(D_METHOD("set_default_damp", "damp"), &ManyBoneIK3D::set_default_damp);
	ClassDB::bind_method(D_METHOD("get_pin_nodepath", "index"), &ManyBoneIK3D::get_pin_nodepath);
	ClassDB::bind_method(D_METHOD("set_pin_nodepath", "index", "nodepath"), &ManyBoneIK3D::set_pin_nodepath);
	ClassDB::bind_method(D_METHOD("get_bone_count"), &ManyBoneIK3D::get_bone_count);
	ClassDB::bind_method(D_METHOD("set_bone_count", "count"), &ManyBoneIK3D::set_bone_count);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton_node_path"), "set_skeleton_node_path", "get_skeleton_node_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "iterations_per_frame", PROPERTY_HINT_RANGE, "1,150,1,or_greater"), "set_iterations_per_frame", "get_iterations_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "default_damp", PROPERTY_HINT_RANGE, "0.01,180.0,0.01,radians,exp", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_default_damp", "get_default_damp");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "edit_constraints", PROPERTY_HINT_ENUM, "Off,Constraints auto-unlock,Constraints lock,Bone damp edit"), "set_edit_constraint_mode", "get_edit_constraint_mode");
}

ManyBoneIK3D::ManyBoneIK3D() {
}

ManyBoneIK3D::~ManyBoneIK3D() {
}

void ManyBoneIK3D::queue_print_skeleton() {
	queue_debug_skeleton = true;
}

float ManyBoneIK3D::get_pin_passthrough_factor(int32_t p_effector_index) const {
	ERR_FAIL_INDEX_V(p_effector_index, pins.size(), 0.0f);
	const Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	return effector_template->get_passthrough_factor();
}

void ManyBoneIK3D::set_pin_passthrough_factor(int32_t p_effector_index, const float p_passthrough_factor) {
	ERR_FAIL_INDEX(p_effector_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	ERR_FAIL_NULL(effector_template);
	effector_template->set_passthrough_factor(p_passthrough_factor);
	set_dirty();
}

void ManyBoneIK3D::set_constraint_count(int32_t p_count) {
	int32_t old_count = constraint_names.size();
	constraint_count = p_count;
	constraint_names.resize(p_count);
	kusudama_twist.resize(p_count);
	kusudama_limit_cone_count.resize(p_count);
	kusudama_limit_cones.resize(p_count);
	for (int32_t constraint_i = p_count; constraint_i-- > old_count;) {
		constraint_names.write[constraint_i] = String();
		kusudama_limit_cone_count.write[constraint_i] = 0;
		kusudama_limit_cones.write[constraint_i].resize(0);
		kusudama_twist.write[constraint_i] = Vector2(Math_PI, Math_TAU - CMP_EPSILON);
	}
	set_dirty();
	notify_property_list_changed();
}

int32_t ManyBoneIK3D::get_constraint_count() const {
	return constraint_count;
}

inline StringName ManyBoneIK3D::get_constraint_name(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, constraint_names.size(), StringName());
	return constraint_names[p_index];
}

void ManyBoneIK3D::set_kusudama_twist(int32_t p_index, Vector2 p_to) {
	ERR_FAIL_INDEX(p_index, constraint_count);
	kusudama_twist.write[p_index] = p_to;
	set_dirty();
}

int32_t ManyBoneIK3D::find_effector_id(StringName p_bone_name) {
	for (int32_t constraint_i = 0; constraint_i < constraint_count; constraint_i++) {
		if (constraint_names[constraint_i] == p_bone_name) {
			return constraint_i;
		}
	}
	return -1;
}

void ManyBoneIK3D::set_kusudama_limit_cone(int32_t p_contraint_index, int32_t p_index,
		Vector3 p_center, float p_radius) {
	ERR_FAIL_INDEX(p_contraint_index, kusudama_limit_cones.size());
	Vector<Vector4> cones = kusudama_limit_cones.write[p_contraint_index];
	if (Math::is_zero_approx(p_center.length_squared())) {
		p_center = Vector3(0, 1, 0);
	}
	Vector3 center = p_center.normalized();
	Vector4 cone;
	cone.x = center.x;
	cone.y = center.y;
	cone.z = center.z;
	cone.w = p_radius;
	cones.write[p_index] = cone;
	kusudama_limit_cones.write[p_contraint_index] = cones;
	set_dirty();
}

Vector3 ManyBoneIK3D::get_kusudama_limit_cone_center(int32_t p_contraint_index, int32_t p_index) const {
	if (unlikely((p_contraint_index) < 0 || (p_contraint_index) >= (kusudama_limit_cone_count.size()))) {
		ERR_PRINT_ONCE("Can't get limit cone center.");
		return Vector3(0.0, 1.0, 0.0);
	}
	if (unlikely((p_contraint_index) < 0 || (p_contraint_index) >= (kusudama_limit_cones.size()))) {
		ERR_PRINT_ONCE("Can't get limit cone center.");
		return Vector3(0.0, 1.0, 0.0);
	}
	if (unlikely((p_index) < 0 || (p_index) >= (kusudama_limit_cones[p_contraint_index].size()))) {
		ERR_PRINT_ONCE("Can't get limit cone center.");
		return Vector3(0.0, 1.0, 0.0);
	}
	const Vector4 &cone = kusudama_limit_cones[p_contraint_index][p_index];
	Vector3 ret;
	ret.x = cone.x;
	ret.y = cone.y;
	ret.z = cone.z;
	return ret;
}

float ManyBoneIK3D::get_kusudama_limit_cone_radius(int32_t p_contraint_index, int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_contraint_index, kusudama_limit_cone_count.size(), Math_TAU);
	ERR_FAIL_INDEX_V(p_contraint_index, kusudama_limit_cones.size(), Math_TAU);
	ERR_FAIL_INDEX_V(p_index, kusudama_limit_cones[p_contraint_index].size(), Math_TAU);
	return kusudama_limit_cones[p_contraint_index][p_index].w;
}

int32_t ManyBoneIK3D::get_kusudama_limit_cone_count(int32_t p_contraint_index) const {
	return kusudama_limit_cone_count[p_contraint_index];
}

void ManyBoneIK3D::set_kusudama_limit_cone_count(int32_t p_contraint_index, int32_t p_count) {
	ERR_FAIL_INDEX(p_contraint_index, kusudama_limit_cone_count.size());
	ERR_FAIL_INDEX(p_contraint_index, kusudama_limit_cones.size());
	int32_t old_cone_count = kusudama_limit_cones[p_contraint_index].size();
	kusudama_limit_cone_count.write[p_contraint_index] = p_count;
	Vector<Vector4> &cones = kusudama_limit_cones.write[p_contraint_index];
	cones.resize(p_count);
	for (int32_t cone_i = p_count; cone_i-- > old_cone_count;) {
		Vector4 &cone = cones.write[cone_i];
		cone.x = 0.0f;
		cone.y = 1.0f;
		cone.z = 0.0f;
		cone.w = Math::deg_to_rad(10.0f);
	}
	notify_property_list_changed();
	set_dirty();
}

real_t ManyBoneIK3D::get_default_damp() const {
	return default_damp;
}

void ManyBoneIK3D::set_default_damp(float p_default_damp) {
	default_damp = p_default_damp;
	set_dirty();
}

StringName ManyBoneIK3D::get_pin_bone_name(int32_t p_effector_index) const {
	ERR_FAIL_INDEX_V(p_effector_index, pins.size(), "");
	Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	return effector_template->get_name();
}

void ManyBoneIK3D::set_kusudama_limit_cone_radius(int32_t p_effector_index, int32_t p_index, float p_radius) {
	ERR_FAIL_INDEX(p_effector_index, kusudama_limit_cone_count.size());
	ERR_FAIL_INDEX(p_effector_index, kusudama_limit_cones.size());
	ERR_FAIL_INDEX(p_index, kusudama_limit_cone_count[p_effector_index]);
	ERR_FAIL_INDEX(p_index, kusudama_limit_cones[p_effector_index].size());
	Vector4 &cone = kusudama_limit_cones.write[p_effector_index].write[p_index];
	cone.w = p_radius;
	set_dirty();
}

void ManyBoneIK3D::set_kusudama_limit_cone_center(int32_t p_effector_index, int32_t p_index, Vector3 p_center) {
	ERR_FAIL_INDEX(p_effector_index, kusudama_limit_cone_count.size());
	ERR_FAIL_INDEX(p_effector_index, kusudama_limit_cones.size());
	ERR_FAIL_INDEX(p_index, kusudama_limit_cones[p_effector_index].size());
	Vector4 &cone = kusudama_limit_cones.write[p_effector_index].write[p_index];
	if (Math::is_zero_approx(p_center.length_squared())) {
		cone.x = 0;
		cone.y = 1;
		cone.z = 0;
	} else {
		cone.x = p_center.x;
		cone.y = p_center.y;
		cone.z = p_center.z;
	}
	set_dirty();
}

Vector2 ManyBoneIK3D::get_kusudama_twist(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, kusudama_twist.size(), Vector2());
	return kusudama_twist[p_index];
}

void ManyBoneIK3D::set_constraint_name(int32_t p_index, String p_name) {
	ERR_FAIL_INDEX(p_index, constraint_names.size());
	if (get_skeleton()) {
		Vector<BoneId> root_bones = get_skeleton()->get_parentless_bones();
		BoneId bone_id = get_skeleton()->find_bone(p_name);
		if (root_bones.find(bone_id) != -1) {
			print_error("The root bone cannot be constrained.");
			p_name = "";
		}
	}
	constraint_names.write[p_index] = p_name;
	set_dirty();
}

Vector<Ref<IKBoneSegment>> ManyBoneIK3D::get_segmented_skeletons() {
	return segmented_skeletons;
}
float ManyBoneIK3D::get_iterations_per_frame() const {
	return iterations_per_frame;
}

void ManyBoneIK3D::set_iterations_per_frame(const float &p_iterations_per_frame) {
	iterations_per_frame = p_iterations_per_frame;
}

void ManyBoneIK3D::set_pin_bone_name(int32_t p_effector_index, StringName p_name) const {
	ERR_FAIL_INDEX(p_effector_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	effector_template->set_name(p_name);
}

void ManyBoneIK3D::set_pin_nodepath(int32_t p_effector_index, NodePath p_node_path) {
	ERR_FAIL_INDEX(p_effector_index, pins.size());
	Node *node = get_node_or_null(p_node_path);
	if (!node) {
		return;
	}
	Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	effector_template->set_target_node(p_node_path);
}

NodePath ManyBoneIK3D::get_pin_nodepath(int32_t p_effector_index) const {
	ERR_FAIL_INDEX_V(p_effector_index, pins.size(), NodePath());
	Ref<IKEffectorTemplate> effector_template = pins[p_effector_index];
	return effector_template->get_target_node();
}

void ManyBoneIK3D::execute(real_t delta) {
	if (!is_visible_in_tree()) {
		return;
	}
	if (get_pin_count() == 0) {
		return;
	}
	if (!segmented_skeletons.size()) {
		set_dirty();
	}
	if (is_dirty) {
		skeleton_changed(get_skeleton());
		is_dirty = false;
		update_gizmos();
	}
	if (bone_list.size()) {
		Ref<IKNode3D> root_ik_bone = bone_list.write[0]->get_ik_transform();
		if (root_ik_bone.is_null()) {
			return;
		}
		Ref<IKNode3D> root_ik_parent_transform = root_ik_bone->get_parent();
		if (root_ik_parent_transform.is_null()) {
			return;
		}
		root_ik_parent_transform->set_global_transform(Transform3D());
	}

	update_ik_bones_transform();
	for (int32_t i = 0; i < get_iterations_per_frame(); i++) {
		for (Ref<IKBoneSegment> segmented_skeleton : segmented_skeletons) {
			if (segmented_skeleton.is_null()) {
				continue;
			}
			segmented_skeleton->segment_solver(bone_damp_cache, constrain_mode);
		}
	}
	update_skeleton_bones_transform();
}

void ManyBoneIK3D::skeleton_changed(Skeleton3D *p_skeleton) {
	if (!p_skeleton) {
		return;
	}
	Vector<int32_t> roots = p_skeleton->get_parentless_bones();
	if (!roots.size()) {
		return;
	}
	bone_list.clear();
	segmented_skeletons.clear();
	bool has_pins = false;
	for (Ref<IKEffectorTemplate> pin : pins) {
		if (pin.is_valid() && !pin->get_name().is_empty()) {
			has_pins = true;
			break;
		}
	}
	if (!has_pins) {
		return;
	}
	for (BoneId root_bone_index : roots) {
		StringName parentless_bone = p_skeleton->get_bone_name(root_bone_index);
		Ref<IKBoneSegment> segmented_skeleton = Ref<IKBoneSegment>(memnew(IKBoneSegment(p_skeleton, parentless_bone, pins, nullptr, root_bone_index, -1)));
		segmented_skeleton->get_root()->get_ik_transform()->set_parent(root_transform);
		segmented_skeleton->generate_default_segments_from_root(pins, root_bone_index, -1);
		Vector<Ref<IKBone3D>> new_bone_list;
		segmented_skeleton->create_bone_list(new_bone_list, true, queue_debug_skeleton);
		bone_list.append_array(new_bone_list);
		Vector<Vector<real_t>> weight_array;
		segmented_skeleton->update_pinned_list(weight_array);
		segmented_skeleton->recursive_create_headings_arrays_for(segmented_skeleton);
		segmented_skeletons.push_back(segmented_skeleton);
	}
	update_ik_bones_transform();
	for (Ref<IKBone3D> ik_bone_3d : bone_list) {
		ik_bone_3d->update_default_bone_direction_transform(p_skeleton);
	}
	for (int constraint_i = 0; constraint_i < constraint_count; constraint_i++) {
		if (unlikely((constraint_i) < 0 || (constraint_i) >= (constraint_names.size()))) {
			break;
		}
		String bone = constraint_names[constraint_i];
		BoneId bone_id = p_skeleton->find_bone(bone);
		for (Ref<IKBone3D> ik_bone_3d : bone_list) {
			if (ik_bone_3d->get_bone_id() != bone_id) {
				continue;
			}
			Ref<IKNode3D> bone_direction_transform;
			bone_direction_transform.instantiate();
			bone_direction_transform->set_parent(ik_bone_3d->get_ik_transform());
			bone_direction_transform->set_transform(Transform3D(Basis(), ik_bone_3d->get_bone_direction_transform()->get_transform().origin));
			Ref<IKKusudama> constraint = Ref<IKKusudama>(memnew(IKKusudama()));
			constraint->enable_orientational_limits();

			if (!(unlikely((constraint_i) < 0 || (constraint_i) >= (kusudama_limit_cone_count.size())))) {
				for (int32_t cone_i = 0; cone_i < kusudama_limit_cone_count[constraint_i]; cone_i++) {
					Ref<IKLimitCone> previous_cone;
					if (cone_i > 0) {
						previous_cone = constraint->get_limit_cones()[cone_i - 1];
					}
					if (unlikely((constraint_i) < 0 || (constraint_i) >= (kusudama_limit_cones.size()))) {
						break;
					}
					const Vector<Vector4> &cones = kusudama_limit_cones[constraint_i];
					if (unlikely((cone_i) < 0 || (cone_i) >= (cones.size()))) {
						break;
					}
					const Vector4 &cone = cones[cone_i];
					constraint->add_limit_cone(Vector3(cone.x, cone.y, cone.z), cone.w);
				}
			}
			const Vector2 axial_limit = get_kusudama_twist(constraint_i);
			constraint->enable_axial_limits();
			constraint->set_axial_limits(axial_limit.x, axial_limit.y);
			ik_bone_3d->add_constraint(constraint);
			constraint->_update_constraint();
			break;
		}
	}
	for (Ref<IKBone3D> ik_bone_3d : bone_list) {
		ik_bone_3d->update_default_constraint_transform();
	}
	bone_damp_cache.clear();
	int32_t bone_damp_count = 0;
	while (bone_damp_count < bone_damp.size()) {
		bone_damp_count++;
		if (unlikely((bone_damp_count) < 0 || (bone_damp_count) >= (bone_damp.size()))) {
			continue;
		}
		Dictionary bone = bone_damp[bone_damp_count];
		if (!bone.has("bone_name") || bone.has("damp")) {
			continue;
		}
		StringName bone_name = bone["bone_name"];
		BoneId bone_id = p_skeleton->find_bone(bone_name);
		bone_damp_cache.insert(bone_id, bone["damp"]);
	}
	if (queue_debug_skeleton) {
		queue_debug_skeleton = false;
	}
}

real_t ManyBoneIK3D::get_pin_weight(int32_t p_pin_index) const {
	ERR_FAIL_INDEX_V(p_pin_index, pins.size(), 0.0);
	const Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	return effector_template->get_weight();
}

void ManyBoneIK3D::set_pin_weight(int32_t p_pin_index, const real_t &p_weight) {
	ERR_FAIL_INDEX(p_pin_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	if (effector_template.is_null()) {
		effector_template.instantiate();
		pins.write[p_pin_index] = effector_template;
	}
	effector_template->set_weight(p_weight);
	set_dirty();
}

Vector3 ManyBoneIK3D::get_pin_direction_priorities(int32_t p_pin_index) const {
	ERR_FAIL_INDEX_V(p_pin_index, pins.size(), Vector3(0, 0, 0));
	const Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	return effector_template->get_direction_priorities();
}

void ManyBoneIK3D::set_pin_direction_priorities(int32_t p_pin_index, const Vector3 &p_priority_direction) {
	ERR_FAIL_INDEX(p_pin_index, pins.size());
	Ref<IKEffectorTemplate> effector_template = pins[p_pin_index];
	if (effector_template.is_null()) {
		effector_template.instantiate();
		pins.write[p_pin_index] = effector_template;
	}
	effector_template->set_direction_priorities(p_priority_direction);
	set_dirty();
}

void ManyBoneIK3D::set_dirty() {
	is_dirty = true;
}

int32_t ManyBoneIK3D::find_constraint(String p_string) const {
	for (int32_t constraint_i = 0; constraint_i < constraint_count; constraint_i++) {
		if (get_constraint_name(constraint_i) == p_string) {
			return constraint_i;
		}
	}
	return -1;
}

Skeleton3D *ManyBoneIK3D::get_skeleton() const {
	Node *node = get_node_or_null(skeleton_node_path);
	if (!node) {
		return nullptr;
	}
	return cast_to<Skeleton3D>(node);
}

NodePath ManyBoneIK3D::get_skeleton_node_path() {
	return skeleton_node_path;
}

void ManyBoneIK3D::set_skeleton_node_path(NodePath p_skeleton_node_path) {
	skeleton_node_path = p_skeleton_node_path;
	set_dirty();
}

void ManyBoneIK3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			set_process_internal(true);
			set_notify_transform(true);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!is_visible_in_tree()) {
				return;
			}
			if (is_dirty) {
				skeleton_changed(get_skeleton());
			}
			execute(get_process_delta_time());
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			update_gizmos();
		} break;
	}
}

void ManyBoneIK3D::remove_constraint(int32_t p_index) {
	ERR_FAIL_INDEX(p_index, constraint_count);

	int32_t old_count = constraint_count;

	constraint_names.remove_at(p_index);
	kusudama_limit_cone_count.remove_at(p_index);
	kusudama_limit_cones.remove_at(p_index);
	kusudama_twist.remove_at(p_index);

	constraint_count--;
	constraint_names.resize(constraint_count);
	kusudama_twist.resize(constraint_count);
	kusudama_limit_cone_count.resize(constraint_count);
	kusudama_limit_cones.resize(constraint_count);

	set_dirty();
	notify_property_list_changed();
}

real_t ManyBoneIK3D::get_kusudama_twist_current(int32_t p_index) {
	ERR_FAIL_INDEX_V(p_index, constraint_names.size(), 0.0f);
	String bone_name = constraint_names[p_index];
	if (!segmented_skeletons.size()) {
		return 0;
	}
	for (Ref<IKBoneSegment> segmented_skeleton : segmented_skeletons) {
		if (segmented_skeleton.is_null()) {
			continue;
		}
		Ref<IKBone3D> ik_bone = segmented_skeleton->get_ik_bone(get_skeleton()->find_bone(bone_name));
		if (ik_bone.is_null()) {
			continue;
		}
		if (ik_bone->get_constraint().is_null()) {
			continue;
		}
		return CLAMP(ik_bone->get_constraint()->get_current_twist_rotation(ik_bone), 0, 1);
	}
	return 0;
}

void ManyBoneIK3D::set_kusudama_twist_current(int32_t p_index, real_t p_rotation) {
	ERR_FAIL_INDEX(p_index, constraint_names.size());
	String bone_name = constraint_names[p_index];
	for (Ref<IKBoneSegment> segmented_skeleton : segmented_skeletons) {
		if (segmented_skeleton.is_null()) {
			continue;
		}
		Ref<IKBone3D> ik_bone = segmented_skeleton->get_ik_bone(get_skeleton()->find_bone(bone_name));
		if (ik_bone.is_null()) {
			continue;
		}
		if (ik_bone->get_constraint().is_null()) {
			continue;
		}
		ik_bone->get_constraint()->set_current_twist_rotation(ik_bone, p_rotation);
		ik_bone->set_skeleton_bone_pose(get_skeleton());
	}
}

int ManyBoneIK3D::get_edit_constraint_mode() const {
	return constrain_mode;
}

void ManyBoneIK3D::set_edit_constraint_mode(int p_value) {
	// TODO: Add tool tip to explain this disables. Or graphical widget.
	// TODO: Toggle slider widget instead of checkbox?
	// Anything which will automatically disable the solver when the user is trying to edit constraints,
	// and re-enables the solver when they say they are done editing constraints.
	// Possibly with a visual hint to indicate that solver is on or off as a result of being in that mode
	constrain_mode = p_value;
	notify_property_list_changed();
}

void ManyBoneIK3D::set_bone_count(int32_t p_count) {
	bone_count = p_count;
	bone_damp.resize(p_count);
	notify_property_list_changed();
}

int32_t ManyBoneIK3D::get_bone_count() const {
	return bone_count;
}

real_t ManyBoneIK3D::get_bone_damp(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, bone_damp.size(), get_default_damp());
	ERR_FAIL_COND_V(!bone_damp[p_index].keys().has("damp"), get_default_damp());
	return bone_damp[p_index]["damp"];
}

void ManyBoneIK3D::set_bone_damp(int32_t p_index, real_t p_damp) {
	ERR_FAIL_INDEX(p_index, bone_damp.size());
	bone_damp.write[p_index]["damp"] = p_damp;
	notify_property_list_changed();
}

StringName ManyBoneIK3D::get_bone_damp_bone_name(int32_t p_index) const {
	ERR_FAIL_INDEX_V(p_index, bone_damp.size(), StringName());
	ERR_FAIL_COND_V(!bone_damp[p_index].keys().has("bone_name"), StringName());
	return bone_damp[p_index]["bone_name"];
}

void ManyBoneIK3D::set_bone_damp_bone_name(int32_t p_index, StringName p_name) {
	ERR_FAIL_INDEX(p_index, bone_damp.size());
	if (get_skeleton()) {
		Vector<BoneId> root_bones = get_skeleton()->get_parentless_bones();
		BoneId bone_id = get_skeleton()->find_bone(p_name);
		if (unlikely(root_bones.find(bone_id) != -1)) {
			print_error("The root bone cannot be constrained.");
			p_name = StringName();
		}
	}
	bone_damp.write[p_index]["bone_name"] = p_name;
	notify_property_list_changed();
}
Vector<Ref<IKBone3D>> ManyBoneIK3D::get_bone_list() {
	return bone_list;
}