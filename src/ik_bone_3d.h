/*************************************************************************/
/*  ik_bone_3d.h                                                         */
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

#ifndef IK_BONE_3D_H
#define IK_BONE_3D_H

#include "ik_bone_3d.h"
#include "ik_effector_template.h"
#include "ik_limit_cone.h"
#include "math/ik_node_3d.h"

#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "scene/3d/skeleton_3d.h"

class IKEffector3D;
class IKKusudama;
class ManyBoneIK3D;

class IKBone3D : public Resource {
	GDCLASS(IKBone3D, Resource);

	BoneId bone_id = -1;
	Ref<IKBone3D> parent;
	Vector<Ref<IKBone3D>> children;
	Ref<IKEffector3D> pin;

	float default_dampening = Math_PI;
	float dampening = get_parent().is_null() ? Math_PI : default_dampening;
	float cos_half_dampen = Math::cos(dampening / 2.0f);
	Ref<IKKusudama> constraint;
	// In the space of the local parent bone transform.
	// The origin is the origin of the bone direction transform
	// Can be independent and should be calculated
	// to keep -y to be the opposite of its bone forward orientation
	// To avoid singularity that is ambiguous.
	Ref<IKNode3D> godot_skeleton_aligned_transform = Ref<IKNode3D>(memnew(IKNode3D())); // The bone's actual transform.
	Ref<IKNode3D> constraint_transform = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> constraint_twist_transform = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> bone_direction_transform = Ref<IKNode3D>(memnew(IKNode3D())); // Physical direction of the bone. Calculate Y is the bone up.

	Ref<IKNode3D> constraint_transform_reset = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> constraint_twist_transform_reset = Ref<IKNode3D>(memnew(IKNode3D()));
	Ref<IKNode3D> bone_direction_transform_reset = Ref<IKNode3D>(memnew(IKNode3D())); // Physical direction of the bone. Calculate Y is the bone up.

protected:
	static void _bind_methods();

public:
	bool is_axially_constrained();
	bool is_orientationally_constrained();
	Transform3D get_bone_direction_global_pose() const;
	Ref<IKNode3D> get_godot_skeleton_aligned_transform();
	Ref<IKNode3D> get_bone_direction_transform();
	Ref<IKNode3D> get_constraint_transform();
	Ref<IKNode3D> get_constraint_twist_transform();
	Ref<IKNode3D> get_bone_direction_transform_reset();
	Ref<IKNode3D> get_constraint_transform_reset();
	Ref<IKNode3D> get_constraint_twist_transform_reset();
	void update_default_bone_direction_transform(Skeleton3D *p_skeleton);
	void update_default_constraint_transform();
	void add_constraint(Ref<IKKusudama> p_constraint);
	Ref<IKKusudama> get_constraint() const;
	void set_bone_id(BoneId p_bone_id, Skeleton3D *p_skeleton = nullptr);
	BoneId get_bone_id() const;
	void set_parent(const Ref<IKBone3D> &p_parent);
	Ref<IKBone3D> get_parent() const;
	void set_pin(const Ref<IKEffector3D> &p_pin);
	Ref<IKEffector3D> get_pin() const;
	void set_global_pose(const Transform3D &p_transform);
	Transform3D get_global_pose() const;
	void set_pose(const Transform3D &p_transform);
	Transform3D get_pose() const;
	void set_initial_pose(Skeleton3D *p_skeleton);
	void set_skeleton_bone_pose(Skeleton3D *p_skeleton);
	void create_pin();
	bool is_pinned() const;
	IKBone3D() {}
	IKBone3D(StringName p_bone, Skeleton3D *p_skeleton, const Ref<IKBone3D> &p_parent, Vector<Ref<IKEffectorTemplate>> &p_pins, float p_default_dampening = Math_PI, ManyBoneIK3D *p_many_bone_ik = nullptr);
	~IKBone3D() {}
	float get_cos_half_dampen() const;
	void set_cos_half_dampen(float p_cos_half_dampen);
};

#endif // IK_BONE_3D_H
