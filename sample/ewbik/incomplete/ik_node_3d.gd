@tool
extends Object

class_name IKNode3D

enum AxisDirection {
    NORMAL = 0,
    IGNORE = 1,
    FORWARD = 2
}

enum Side {
    RIGHT = 1,
    LEFT = -1
}

enum Axis {
    X = 0,
    Y = 1,
    Z = 2
}

var debug: bool = false

var localMBasis: IKBasis
var globalMBasis: IKBasis
var parent: IKNode3D = null

var slipType: int = 0
var dirty: bool = true

var dependentsSet: WeakRefSet = WeakRefSet.new()
var dependentsMap: Dictionary = {}

var workingVector: Vector3

var areGlobal: bool = true
var tag: String = str(hash(self)) + "-Axes"

func create_temp_vars(type: Vector3) -> void:
	workingVector = type.copy()

func _init(globalBasis: IKBasis, parent: IKNode3D) -> void:
	self.globalMBasis = globalBasis.copy()
	create_temp_vars(globalBasis.get_origin())
	
	if parent != null:
		set_parent(parent)
	else:
		self.areGlobal = true
		self.localMBasis = globalBasis.copy()

	update_global()

func make_default_vec() -> Vector3:
	return Vector3()


func _init(origin: Vector3, inX: Vector3, inY: Vector3, inZ: Vector3, parent: IKNode3D, customBases: bool = false) -> void:
	if not customBases:
		globalMBasis = parent.get_global_m_basis().copy() if parent != null else AffineBasis(origin)
		localMBasis = parent.get_local_m_basis().copy() if parent != null else AffineBasis(origin)
		globalMBasis.set_identity()
		localMBasis.set_identity()

	if parent == null:
		self.areGlobal = true

	self.parent = parent


func get_parent_axes() -> IKNode3D:
	return self.parent


func update_global(force: bool = false) -> void:
	if self.dirty or force:
		if self.areGlobal:
			globalMBasis.adopt_values(self.localMBasis)
		else:
			get_parent_axes().update_global(false)
			get_parent_axes().get_global_m_basis().set_to_global_of(self.localMBasis, self.globalMBasis)

	dirty = false


func debug_call() -> void:
	pass


func origin_() -> Vector3:
	self.update_global()
	var temp_origin := self.get_global_m_basis().get_origin()
	return temp_origin


func set_parent(intended_parent: IKNode3D, requested_by: Object = null) -> void:
	self.update_global()
	var old_parent := self.get_parent_axes()

	for dependent in dependentsSet:
		dependent.parent_change_warning(self, old_parent, intended_parent, requested_by)

	if intended_parent != null and intended_parent != self:
		intended_parent.update_global()
		intended_parent.get_global_m_basis().set_to_local_of(globalMBasis, localMBasis)

		if old_parent != null:
			old_parent.disown(self)

		self.parent = intended_parent
		self.get_parent_axes().register_dependent(self)
		self.areGlobal = false
	else:
		if old_parent != null:
			old_parent.disown(self)

		self.parent = null
		self.areGlobal = true

	self.mark_dirty()
	self.update_global()

	for dependent in dependentsSet:
		dependent.parent_change_completion_notice(self, old_parent, intended_parent, requested_by)


func for_each_dependent(action: Callable) -> void:
	var i := 0
	while i < dependentsSet.size():
		var dr := dependentsSet[i]
		if dr != null:
			action.call(dr)
		else:
			dependentsSet.remove(i)
			continue

		i += 1


func update_global(force: bool = false) -> void:
	if self.dirty or force:
		if self.areGlobal:
			globalMBasis.adopt_values(self.localMBasis)
		else:
			get_parent_axes().update_global(false)
			get_parent_axes().get_global_m_basis().set_to_global_of(self.localMBasis, self.globalMBasis)

	dirty = false

func get_global_chirality() -> int:
	self.update_global()
	return self.get_global_m_basis().chirality

func get_local_chirality() -> int:
	self.update_global()
	return self.get_local_m_basis().chirality

func is_global_axis_flipped(axis: int) -> bool:
	self.update_global()
	return globalMBasis.is_axis_flipped(axis)

func is_local_axis_flipped(axis: int) -> bool:
	return localMBasis.is_axis_flipped(axis)

func set_relative_to_parent(par: IKNode3D) -> void:
	if self.get_parent_axes() != null:
		self.get_parent_axes().disown(self)

	self.parent = par
	self.areGlobal = false
	self.get_parent_axes().register_dependent(self)
	self.mark_dirty()

func needs_update() -> bool:
	return self.dirty

func get_global_of(input_vector: Vector3) -> Vector3:
	var result := input_vector.duplicate()
	set_to_global_of(input_vector, result)
	return result


public IKRay3D getGlobalOf(IKRay3D in) {
	return new IKRay3D(this.getGlobalOf(in.p1()), this.getGlobalOf(in.p2()));
}

/**
	* resets this IKNode3D to be equivalent to the identity transform and marks
	* it dirty
	*/
public void toIdentity() {
	this.localMBasis.setIdentity();
	this.markDirty();
}


public Vector3 getLocalOf(Vector3 in) {
	this.updateGlobal();
	return getGlobalMBasis().getLocalOf(in);
}

/**
	* Given a vector in global coordinates, modifies the vector's values to
	* represent its position in theseAxes local coordinates.
	* 
	* @param in
	* @return a reference to the @param in object.
	*/

public Vector3 setToLocalOf(Vector3 in) {
	this.updateGlobal();
	Vector3 result = (Vector3) in.copy();
	this.getGlobalMBasis().setToLocalOf(in, result);
	in.set(result);
	return result;
}

/**
	* Given a vector in global coordinates, modifies the vector's values to
	* represent its position in theseAxes local coordinates.
	* 
	* @param in
	*/

public void setToLocalOf(Vector3 in, Vector3 out) {
	this.updateGlobal();
	this.getGlobalMBasis().setToLocalOf(in, out);
}

/**
	* Given a sgRay in global coordinates, modifies the sgRay's values to represent
	* its position in theseAxes local coordinates.
	* 
	* @param in
	*/

public void setToLocalOf(IKRay3D in, IKRay3D out) {
	this.setToLocalOf(in.p1(), out.p1());
	this.setToLocalOf(in.p2(), out.p2());
}

public void setToLocalOf(IKBasis input, IKBasis output) {
	this.updateGlobal();
	this.getGlobalMBasis().setToLocalOf(input, output);
}

public <R extends IKRay3D> R getLocalOf(R in) {
	R result = (R) in.copy();
	result.p1.set(this.getLocalOf(in.p1()));
	result.p2.set(this.getLocalOf(in.p2()));
	return result;
}

public void translateByLocal(Vector3 translate) {
	this.updateGlobal();
	getLocalMBasis().translateBy(translate);
	this.markDirty();

}

public void translateByGlobal(Vector3 translate) {
	if (this.getParentAxes() != null) {
		this.updateGlobal();
		this.translateTo(translate.addCopy(this.origin_()));
	} else {
		getLocalMBasis().translateBy(translate);
	}

	this.markDirty();
}

public void translateTo(Vector3 translate, boolean slip) {
	this.updateGlobal();
	if (slip) {
		IKNode3D tempAbstractAxes = this.getGlobalCopy();
		tempAbstractAxes.translateTo(translate);
		this.slipTo(tempAbstractAxes);
	} else {
		this.translateTo(translate);
	}
}

public void translateTo(Vector3 translate) {
	if (this.getParentAxes() != null) {
		this.updateGlobal();
		getLocalMBasis().translateTo(getParentAxes().getGlobalMBasis().getLocalOf(translate));
		this.markDirty();
	} else {
		this.updateGlobal();
		getLocalMBasis().translateTo(translate);
		this.markDirty();
	}

}

public void setSlipType(int type) {
	if (this.getParentAxes() != null) {
		if (type == IGNORE) {
			this.getParentAxes().dependentsSet.remove(this);
		} else if (type == NORMAL || type == FORWARD) {
			this.getParentAxes().registerDependent(this);
		}
	}
	this.slipType = type;
}

public int getSlipType() {
	return this.slipType;
}

public void rotateAboutX(double angle, boolean orthonormalized) {
	this.updateGlobal();
	Quaternion xRot = new Quaternion(getGlobalMBasis().getXHeading(), angle);
	this.rotateBy(xRot);
	this.markDirty();
}

public void rotateAboutY(double angle, boolean orthonormalized) {
	this.updateGlobal();
	Quaternion yRot = new Quaternion(getGlobalMBasis().getYHeading(), angle);
	this.rotateBy(yRot);
	this.markDirty();
}

public void rotateAboutZ(double angle, boolean orthonormalized) {
	this.updateGlobal();
	Quaternion zRot = new Quaternion(getGlobalMBasis().getZHeading(), angle);
	this.rotateBy(zRot);
	this.markDirty();
}

/**
	* Rotates the bases around their origin in global coordinates
	* 
	* @param rotation
	*/
public void rotateBy(Quaternion apply) {

	this.updateGlobal();
	if (this.getParentAxes() != null) {
		Quaternion newRot = this.getParentAxes().getGlobalMBasis().getLocalOfRotation(apply);
		this.getLocalMBasis().rotateBy(newRot);
	} else {
		this.getLocalMBasis().rotateBy(apply);
	}

	this.markDirty();
}

/**
	* rotates the bases around their origin in Local coordinates
	* 
	* @param rotation
	*/
public void rotateByLocal(Quaternion apply) {
	this.updateGlobal();
	if (parent != null) {
		this.getLocalMBasis().rotateBy(apply);
	}
	this.markDirty();
}

/**
	* sets these axes to have the same orientation and location relative to their
	* parent
	* axes as the input's axes do to the input's parent axes.
	* 
	* If the axes on which this function is called are orthonormal,
	* this function normalizes and orthogonalizes them regardless of whether the
	* targetAxes are orthonormal.
	* 
	* @param targetAxes the Axes to make this Axis identical to
	*/
public void alignLocalsTo(IKNode3D targetAxes) {
	this.getLocalMBasis().adoptValues(targetAxes.localMBasis);
	this.markDirty();
}

/**
	* sets these axes to have the same global orientation as the input Axes.
	* these Axes lx, ly, and lz headings will differ from the target ages,
	* but its gx, gy, and gz headings should be identical unless this
	* axis is orthonormalized and the target axes are not.
	* 
	* @param targetAxes
	*/
public void alignGlobalsTo(IKNode3D targetAxes) {
	targetAxes.updateGlobal();
	this.updateGlobal();
	if (this.getParentAxes() != null) {
		getParentAxes().getGlobalMBasis().setToLocalOf(targetAxes.globalMBasis, localMBasis);
	} else {
		this.getLocalMBasis().adoptValues(targetAxes.globalMBasis);
	}
	this.markDirty();
	this.updateGlobal();
}

public void alignOrientationTo(IKNode3D targetAxes) {
	targetAxes.updateGlobal();
	this.updateGlobal();
	if (this.getParentAxes() != null) {
		this.getGlobalMBasis().rotateTo(targetAxes.getGlobalMBasis().rotation);
		getParentAxes().getGlobalMBasis().setToLocalOf(this.globalMBasis, this.localMBasis);
	} else {
		this.getLocalMBasis().rotateTo(targetAxes.getGlobalMBasis().rotation);
	}
	this.markDirty();
}

/**
	* updates the axes object such that its global orientation
	* matches the given Quaternion object.
	* 
	* @param rotation
	*/
public void setGlobalOrientationTo(Quaternion rotation) {
	this.updateGlobal();
	if (this.getParentAxes() != null) {
		this.getGlobalMBasis().rotateTo(rotation);
		getParentAxes().getGlobalMBasis().setToLocalOf(this.globalMBasis, this.localMBasis);
	} else {
		this.getLocalMBasis().rotateTo(rotation);
	}
	this.markDirty();
}

public void setLocalOrientationTo(Quaternion rotation) {
	this.getLocalMBasis().rotateTo(rotation);
	this.markDirty();
}

public void registerDependent(IKNode3D newDependent) {
	// Make sure we don't hit a dependency loop
	if (IKNode3D.class.isAssignableFrom(newDependent.getClass())) {
		if (((IKNode3D) newDependent).isAncestorOf(this)) {
			this.transferToParent(((IKNode3D) newDependent).getParentAxes());
		}
	}
	if (!dependentsSet.contains(newDependent)) {
		dependentsSet.add(newDependent);
	}
}

public boolean isAncestorOf(IKNode3D potentialDescendent) {
	boolean result = false;
	IKNode3D cursor = potentialDescendent.getParentAxes();
	while (cursor != null) {
		if (cursor == this) {
			result = true;
			break;
		} else {
			cursor = cursor.getParentAxes();
		}
	}
	return result;
}

/**
	* unregisters this IKNode3D from its current parent and
	* registers it to a new parent without changing its global position or
	* orientation
	* when doing so.
	* 
	* @param newParent
	*/

public void transferToParent(IKNode3D newParent) {
	this.emancipate();
	this.setParent(newParent);
}

/**
	* unregisters this IKNode3D from its parent,
	* but keeps its global position the same.
	*/
public void emancipate() {
	if (this.getParentAxes() != null) {
		this.updateGlobal();
		IKNode3D oldParent = this.getParentAxes();
		for (IKNode3D ad : this.dependentsSet) {
			ad.parentChangeWarning(this, this.getParentAxes(), null, null);
		}
		this.getLocalMBasis().adoptValues(this.globalMBasis);
		this.getParentAxes().disown(this);
		this.parent = new DependencyReference<IKNode3D>(null);
		this.areGlobal = true;
		this.markDirty();
		this.updateGlobal();
		for (IKNode3D ad : this.dependentsSet) {
			ad.parentChangeCompletionNotice(this, oldParent, null, null);
		}
	}
}

public void disown(IKNode3D child) {
	dependentsSet.remove(child);
}

public IKBasis getGlobalMBasis() {
	this.updateGlobal();
	return globalMBasis;
}

public IKBasis getLocalMBasis() {
	return localMBasis;
}

public void axisSlipWarning(IKNode3D globalPriorToSlipping, IKNode3D globalAfterSlipping,
		IKNode3D actualAxis, ArrayList<Object> dontWarn) {
	this.updateGlobal();
	if (this.slipType == NORMAL) {
		if (this.getParentAxes() != null) {
			IKNode3D globalVals = this.relativeTo(globalPriorToSlipping);
			globalVals = globalPriorToSlipping.getLocalOf(globalVals);
			this.getLocalMBasis().adoptValues(globalMBasis);
			this.markDirty();
		}
	} else if (this.slipType == FORWARD) {
		IKNode3D globalAfterVals = this.relativeTo(globalAfterSlipping);
		this.notifyDependentsOfSlip(globalAfterVals, dontWarn);
	}
}

public void axisSlipWarning(IKNode3D globalPriorToSlipping, IKNode3D globalAfterSlipping,
		IKNode3D actualAxis) {

}

public void axisSlipCompletionNotice(IKNode3D globalPriorToSlipping, IKNode3D globalAfterSlipping,
		IKNode3D thisAxis) {

}

public void slipTo(IKNode3D newAxisGlobal) {
	this.updateGlobal();
	IKNode3D originalGlobal = this.getGlobalCopy();
	notifyDependentsOfSlip(newAxisGlobal);
	IKNode3D newVals = newAxisGlobal.freeCopy();

	if (this.getParentAxes() != null) {
		newVals = getParentAxes().getLocalOf(newVals);
	}
	this.getLocalMBasis().adoptValues(newVals.globalMBasis);
	this.dirty = true;
	this.updateGlobal();

	notifyDependentsOfSlipCompletion(originalGlobal);
}

/**
	* You probably shouldn't touch this unless you're implementing i/o or
	* undo/redo.
	* 
	* @return
	*/
protected DependencyReference<IKNode3D> getWeakRefToParent() {
	return this.parent;
}

/**
	* You probably shouldn't touch this unless you're implementing i/o or
	* undo/redo.
	* 
	* @return
	*/
protected void setWeakRefToParent(DependencyReference<IKNode3D> parentRef) {
	this.parent = parentRef;
}


func slip_to(new_axis_global: IKNode3D, dont_warn: Array = []) -> void:
	self.update_global()
	var original_global := self.get_global_copy()
	notify_dependents_of_slip(new_axis_global, dont_warn)
	var new_vals := new_axis_global.get_global_copy()

	if self.get_parent_axes() != null:
		new_vals = get_parent_axes().get_local_of(new_axis_global)

	self.align_globals_to(new_axis_global)
	self.mark_dirty()
	self.update_global()

	notify_dependents_of_slip_completion(original_global, dont_warn)

func notify_dependents_of_slip(new_axis_global: IKNode3D, dont_warn: Array) -> void:
	for dependant in dependentsSet:
		if not dont_warn.has(dependant):
			dependant.axis_slip_warning(self.get_global_copy(), new_axis_global, self, dont_warn)
		else:
			print("skipping: ", dependant)

func notify_dependents_of_slip_completion(global_axis_prior_to_slipping: IKNode3D, dont_warn: Array) -> void:
	for dependant in dependentsSet:
		if not dont_warn.has(dependant):
			dependant.axis_slip_completion_notice(global_axis_prior_to_slipping, self.get_global_copy(), self)
		else:
			print("skipping: ", dependant)

func mark_dirty(depth: int = -1) -> void:
	if not self.dirty:
		self.dirty = true
		self.mark_dependents_dirty(depth - 1)

func mark_dependents_dirty(depth: int) -> void:
	if depth >= 0:
		for dependant in dependentsSet:
			dependant.mark_dirty(depth)

func print() -> String:
	self.update_global()
	var global := "Global: " + get_global_m_basis().print()
	var local := "Local: " + get_local_m_basis().print()
	return global + "\n" + local