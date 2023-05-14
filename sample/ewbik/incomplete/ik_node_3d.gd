@tool
extends Object

class_name IKNode3D

public class IKNode3D {
	public static final int NORMAL = 0, IGNORE = 1, FORWARD = 2;
	public static final int RIGHT = 1, LEFT = -1;
	public static final int X = 0, Y = 1, Z = 2;

	public static boolean debug = false;

	public IKBasis localMBasis;
	public IKBasis globalMBasis;
	private DependencyReference<IKNode3D> parent = null;

	private int slipType = 0;
	public boolean dirty = true;

	public WeakHashSet<IKNode3D> dependentsSet = new WeakHashSet<IKNode3D>();
	public WeakHashMap<IKNode3D, Boolean> dependentsMap = new WeakHashMap<IKNode3D, Boolean>();

	protected Vector3 workingVector;

	protected boolean areGlobal = true;
	public String tag = this.getIdentityHash() + "-Axes";

	public void createTempVars(Vector3 type) {
		workingVector = type.copy();
		tempOrigin = type.copy();
	}

	/**
	 * @param globalMBasis a Basis object for this Axes to adopt the vakues of
	 * @param customBases  set to true if you intend to use a custom Bases class, in
	 *                     which case, this constructor will not initialize them.
	 */
	public Vector3 IKNode3D(IKBasis globalBasis, IKNode3D parent) {
		this.globalMBasis = globalBasis.copy();
		createTempVars(globalBasis.getOrigin());
		if (this.getParentAxes() != null)
			setParent(parent);
		else {
			this.areGlobal = true;
			this.localMBasis = globalBasis.copy();
		}

		this.updateGlobal();
	}

	/**
	 * meant to be overriden to initializes an empty vector of
	 * whatever type your extended library extends Vec3d into.
	 * 
	 * If not overriden, will create an empty SGVec3d
	 */
	public Vector3 makeDefaultVec() {
		return (Vector3) new Vector3();
	}

	/**
	 * @param origin              the center of this axes basis. The basis vector
	 *                            parameters will be automatically ADDED to the
	 *                            origin in order to create this basis vector.
	 * @param inX                 the direction of the X basis vector in global
	 *                            coordinates, given as an offset from this base's
	 *                            origin in global coordinates.
	 * @param inY                 the direction of the Y basis vector in global
	 *                            coordinates, given as an offset from this base's
	 *                            origin in global coordinates.
	 * @param inZ                 the direction of the Z basis vector in global
	 *                            coordinates, given as an offset from this base's
	 *                            origin in global coordinates.
	 * @param forceOrthoNormality
	 * @param customBases         set to true if you intend to use a custom Bases
	 *                            class, in which case, this constructor will not
	 *                            initialize them.
	 */
	public IKNode3D(Vector3 origin, Vector3 inX, Vector3 inY, Vector3 inZ, IKNode3D parent,
			boolean customBases) {
		if (!customBases) {
			globalMBasis = parent != null ? parent.getGlobalMBasis().copy() : new AffineBasis(origin);
			localMBasis = parent != null ? parent.getLocalMBasis().copy() : new AffineBasis(origin);
			globalMBasis.setIdentity();
			localMBasis.setIdentity();
		}
		if (parent == null)
			this.areGlobal = true;
	}

	public IKNode3D getParentAxes() {
		if (this.parent == null)
			return null;
		else
			return this.parent.get();
	}

	public void updateGlobal() {
		updateGlobal(false);
	}

	/**
	 * Updates the global coordinates of this transform
	 * 
	 * @param The force boolean is to help you do things you really probably
	 *            shouldn't,
	 *            and will non-recursively update the global transformation cache
	 *            even if it isn't dirty.
	 *            (Meaning, the update won't also be forced on any ancestors.)
	 * 
	 */
	public void updateGlobal(boolean force) {
		if (this.dirty || force) {
			if (this.areGlobal) {
				globalMBasis.adoptValues(this.localMBasis);
			} else {
				getParentAxes().updateGlobal(false);
				getParentAxes().getGlobalMBasis().setToGlobalOf(this.localMBasis, this.globalMBasis);
			}
		}
		dirty = false;
	}

	public void debugCall() {
	};

	Vector3 tempOrigin;

	public Vector3 origin_() {
		this.updateGlobal();
		tempOrigin.set(this.getGlobalMBasis().getOrigin());
		return tempOrigin;
	}

	/**
	 * Sets the parentAxes for this axis globally.
	 * in other words, globalX, globalY, and globalZ remain unchanged, but lx, ly,
	 * and lz
	 * change.
	 * 
	 * @param par the new parent Axes
	 **/
	public void setParent(IKNode3D par) {
		setParent(par, null);
	}

	/**
	 * Sets the parentAxes for this axis globally.
	 * in other words, globalX, globalY, and globalZ remain unchanged, but lx, ly,
	 * and lz change.
	 *
	 * @param intendedParent the new parent Axes
	 * @param requestedBy    the object making thisRequest, will be passed on to
	 *                       parentChangeWarning
	 *                       for any AxisDependancy objects registered with this
	 *                       IKNode3D (can be null if not important)
	 **/
	public void setParent(IKNode3D intendedParent, Object requestedBy) {
		this.updateGlobal();
		IKNode3D oldParent = this.getParentAxes();

		forEachDependent(
				(ad) -> ad.parentChangeWarning(this, oldParent, intendedParent, requestedBy));

		if (intendedParent != null && intendedParent != this) {
			intendedParent.updateGlobal();
			intendedParent.getGlobalMBasis().setToLocalOf(globalMBasis, localMBasis);

			if (oldParent != null) {
				oldParent.disown(this);
			}
			this.parent = new DependencyReference<IKNode3D>(intendedParent);

			this.getParentAxes().registerDependent(this);
			this.areGlobal = false;
		} else {
			if (oldParent != null) {
				oldParent.disown(this);
			}
			this.parent = new DependencyReference<IKNode3D>(null);
			this.areGlobal = true;
		}
		this.markDirty();
		this.updateGlobal();

		forEachDependent(
				(ad) -> ad.parentChangeCompletionNotice(this, oldParent, intendedParent, requestedBy));
	}

	/**
	 * runs the given runnable on each dependent axis,
	 * taking advantage of the call to remove entirely any
	 * weakreferences to elements that have been cleaned up by the garbage
	 * collector.
	 * 
	 * @param r
	 */
	public void forEachDependent(Consumer<IKNode3D> action) {
		Iterator i = dependentsSet.iterator();
		while (i.hasNext()) {
			IKNode3D dr = (IKNode3D) i.next();
			if (dr != null) {
				action.accept(dr);
			} else {
				i.remove();
			}
		}
	}

	public int getGlobalChirality() {
		this.updateGlobal();
		return this.getGlobalMBasis().chirality;
	}

	public int getLocalChirality() {
		this.updateGlobal();
		return this.getLocalMBasis().chirality;
	}

	/**
	 * True if the input axis of this Axes object in global coordinates should be
	 * multiplied by negative one after rotation.
	 * By default, this always returns false. But can be overriden for more advanced
	 * implementations
	 * allowing for reflection transformations.
	 * 
	 * @param axis
	 * @return true if axis should be flipped, false otherwise. Default is false.
	 */
	public boolean isGlobalAxisFlipped(int axis) {
		this.updateGlobal();
		return globalMBasis.isAxisFlipped(axis);
	}

	/**
	 * True if the input axis of this Axes object in local coordinates should be
	 * multiplied by negative one after rotation.
	 * By default, this always returns false. But can be overriden for more advanced
	 * implementations
	 * allowing for reflection transformations.
	 * 
	 * @param axis
	 * @return true if axis should be flipped, false otherwise. Default is false.
	 */
	public boolean isLocalAxisFlipped(int axis) {
		return localMBasis.isAxisFlipped(axis);
	}

	/**
	 * Sets the parentAxes for this axis locally.
	 * in other words, lx,ly,lz remain unchanged, but globalX, globalY, and globalZ
	 * change.
	 * 
	 * if setting this parent would result in a dependency loop, then the input Axes
	 * parent is set to this Axes' parent, prior to this axes setting the input axes
	 * as its parent.
	 **/
	public void setRelativeToParent(IKNode3D par) {
		if (this.getParentAxes() != null)
			this.getParentAxes().disown(this);
		this.parent = new DependencyReference<IKNode3D>(par);
		this.areGlobal = false;
		this.getParentAxes().registerDependent(this);
		this.markDirty();
	}

	public boolean needsUpdate() {
		if (this.dirty)
			return true;
		else
			return false;
	}

	/**
	 * Given a vector in this axes local coordinates, returns the vector's position
	 * in global coordinates.
	 * 
	 * @param in
	 * @return
	 */
	public Vector3 getGlobalOf(Vector3 in) {
		Vector3 result = (Vector3) in.copy();
		setToGlobalOf(in, result);
		return result;
	}

	/**
	 * Given a vector in this axes local coordinates, modifies the vector's values
	 * to represent its position global coordinates.
	 * 
	 * @param in
	 * @return a reference to this the @param in object.
	 */
	public Vector3 setToGlobalOf(Vector3 in) {
		this.updateGlobal();
		getGlobalMBasis().setToGlobalOf(in, in);
		return in;
	}

	/**
	 * Given an input vector in this axes local coordinates, modifies the output
	 * vector's values to represent the input's position in global coordinates.
	 * 
	 * @param in
	 */
	public void setToGlobalOf(Vector3 input, Vector3 output) {
		this.updateGlobal();
		getGlobalMBasis().setToGlobalOf(input, output);
	}

	/**
	 * Given an input sgRay in this axes local coordinates, modifies the output
	 * Rays's values to represent the input's in global coordinates.
	 * 
	 * @param in
	 */
	public void setToGlobalOf(IKRay3D input, IKRay3D output) {
		this.updateGlobal();
		this.setToGlobalOf(input.p1(), output.p1());
		this.setToGlobalOf(input.p2(), output.p2());
	}

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

	public void slipTo(IKNode3D newAxisGlobal, ArrayList<Object> dontWarn) {
		this.updateGlobal();
		IKNode3D originalGlobal = this.getGlobalCopy();
		notifyDependentsOfSlip(newAxisGlobal, dontWarn);
		IKNode3D newVals = newAxisGlobal.getGlobalCopy();

		if (this.getParentAxes() != null) {
			newVals = getParentAxes().getLocalOf(newAxisGlobal);
		}
		this.alignGlobalsTo(newAxisGlobal);
		this.markDirty();
		this.updateGlobal();

		notifyDependentsOfSlipCompletion(originalGlobal, dontWarn);
	}

	public void notifyDependentsOfSlip(IKNode3D newAxisGlobal, ArrayList<Object> dontWarn) {
		for (IKNode3D dependant : dependentsSet) {
			if (!dontWarn.contains(dependant)) {
				// First we check if the dependent extends IKNode3D
				// so we know whether or not to pass the dontWarn list
				if (this.getClass().isAssignableFrom(dependant.getClass())) {
					((IKNode3D) dependant).axisSlipWarning(this.getGlobalCopy(), newAxisGlobal, this, dontWarn);
				} else {
					dependant.axisSlipWarning(this.getGlobalCopy(), newAxisGlobal, this);
				}
			} else {
				System.out.println("skipping: " + dependant);
			}
		}
	}

	public void notifyDependentsOfSlipCompletion(IKNode3D globalAxisPriorToSlipping, ArrayList<Object> dontWarn) {
		for (IKNode3D dependant : dependentsSet) {
			if (!dontWarn.contains(dependant))
				dependant.axisSlipCompletionNotice(globalAxisPriorToSlipping, this.getGlobalCopy(), this);
			else
				System.out.println("skipping: " + dependant);
		}
	}

	public void notifyDependentsOfSlip(IKNode3D newAxisGlobal) {
		for (IKNode3D dependant : dependentsSet) {
			dependant.axisSlipWarning(this.getGlobalCopy(), newAxisGlobal, this);
		}
	}

	public void notifyDependentsOfSlipCompletion(IKNode3D globalAxisPriorToSlipping) {
		for (IKNode3D dependant : dependentsSet) {
			dependant.axisSlipCompletionNotice(globalAxisPriorToSlipping, this.getGlobalCopy(), this);
		}
	}

	/**
	 * @param depth indicates how many descendant generations down to notify of the
	 *              dirtiness.
	 *              leave blank unless you know what you're doing.
	 */
	public void markDirty(int depth) {
		if (!this.dirty) {
			this.dirty = true;
			this.markDependentsDirty(depth - 1);
		}
	}

	public void markDependentsDirty(int depth) {
		if (depth >= 0)
			forEachDependent((a) -> a.markDirty(depth));
	}

	public void markDependentsDirty() {
		forEachDependent((a) -> a.markDirty());
	}

	public void markDirty() {
		if (!this.dirty) {
			this.dirty = true;
			this.markDependentsDirty();
		}
	}

	public String print() {
		this.updateGlobal();
		String global = "Global: " + getGlobalMBasis().print();
		String local = "Local: " + getLocalMBasis().print();
		return global + "\n" + local;
	}
}