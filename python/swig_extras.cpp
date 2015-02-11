/*
    Derived classes for use with swig bindings.
    
    Mainly to provide virtual members in order to provide a cleaner, more object-oriented style.
    
    This is only to be used with swig, so it includes both the definition and implementation of
    classes to be included directly in the generated wrapper.
    
    All necessary headers are assumed to have been included by the .i file.
    
    Unfortunately, we need to do a bit of a hack to handle callback functions.  The standard callbacks
    have no "this" parameter, so we have to provide it in a global.  The global is set by the
    ctor, so there can obviously be only one instance of GCodeInterpreter.
*/

/*
typedef void G_COMPLETE_CALLBACK(int status, int lineno, int sequence_number, const char *err);
typedef void G_STATUS_CALLBACK(int line_no, const char *msg);
typedef int G_USER_CALLBACK(const char *msg);
typedef int G_M_USER_CALLBACK(int mCode);
*/

class GCodeInterpreter;
struct Action;

GCodeInterpreter * _theInterpreter;

class GCodeInterpreter: public CGCodeInterpreter
{
public:
    GCodeInterpreter(CCoordMotion *CM);
	virtual ~GCodeInterpreter(void) {_theInterpreter = NULL;}
	
	// Virtual functions for "callbacks"
	virtual void HandleStatus(int line_no, const char *msg) {}
	virtual void HandleComplete(int status, int lineno, int sequence_number, const char *err) {}
	virtual int HandleUser(const char *msg) {return 0;}
	virtual int HandleMCode(int mCode) {return 0;}
	int Interpret(
				  int board_type,
		          const char *fname,
			      int start, int end,
				  int restart);

    // Interface to action setting.  Return 0 if OK, 1 if bad action number.
    int SetActionNone(int action_num);
    int SetActionSetBit(int action_num, int bit, int state);
    int SetActionSetTwoBits(int action_num, int bit, int state, int bit2, int state2);
    int SetActionDAC(int action_num, int dacnum, double scale, double offset, int minval, int maxval);
    int SetActionProgram(int action_num, const char * name, int thread, int ipersist = -1);
    int SetActionProgramWait(int action_num, const char * name, int thread, int ipersist = -1);
    int SetActionProgramWaitSync(int action_num, const char * name, int thread, int ipersist = -1);
    int SetActionProgramPC(int action_num, const char * name);
    int SetActionCallback(int action_num);
    int SetActionWaitbit(int action_num, int bit, int state);
    
    // Motion parameter setting.  For language bindings, it is easier to have separate get/set rather than
    // directly manipulating a *ptr struct.  Use idiom (in e.g. Python) like:
    // mp = my_gcode_interp.GetMotionParams()
    // mp.BreakAngle = 0.02
    // my_gcode_interp.SetMotionParams(mp)
    void SetMotionParams(const MOTION_PARAMS & m) { CoordMotion->Kinematics->m_MotionParams = m; }
protected:
    int setAction(int action_num, int type, double p1 = 0., double p2 = 0., double p3 = 0., double p4 = 0., double p5 = 0., const char * str = NULL);
};


int GCodeInterpreter::setAction(int action_num, int type, double p1, double p2, double p3, double p4, double p5, const char * str)
{
    if ((unsigned)action_num >= MAX_MCODE_ACTIONS)
        return 1;
    MCODE_ACTION * m = McodeActions + action_num;
    m->Action = type;
    m->dParams[0] = p1;
    m->dParams[1] = p2;
    m->dParams[2] = p3;
    m->dParams[3] = p4;
    m->dParams[4] = p5;
    if (str) {
        strncpy(m->String, str, sizeof(m->String)-1);
        m->String[sizeof(m->String)-1] = 0;
    }
    else
        m->String[0] = 0;
    return 0;
}

int GCodeInterpreter::SetActionNone(int action_num)
{
    return setAction(action_num, M_Action_None);
}

int GCodeInterpreter::SetActionSetBit(int action_num, int bit, int state)
{
    return setAction(action_num, M_Action_Setbit, bit, state);
}

int GCodeInterpreter::SetActionSetTwoBits(int action_num, int bit, int state, int bit2, int state2)
{
    return setAction(action_num, M_Action_Setbit, bit, state, bit2, state2);
}

int GCodeInterpreter::SetActionDAC(int action_num, int dacnum, double scale, double offset, int minval, int maxval)
{
    return setAction(action_num, M_Action_DAC, dacnum, scale, offset, minval, maxval);
}

int GCodeInterpreter::SetActionProgram(int action_num, const char * name, int thread, int ipersist)
{
    return setAction(action_num, M_Action_Program, thread, ipersist, 0,0,0, name);
}

int GCodeInterpreter::SetActionProgramWait(int action_num, const char * name, int thread, int ipersist)
{
    return setAction(action_num, M_Action_Program_wait, thread, ipersist, 0,0,0, name);
}

int GCodeInterpreter::SetActionProgramWaitSync(int action_num, const char * name, int thread, int ipersist)
{
    return setAction(action_num, M_Action_Program_wait_sync, thread, ipersist, 0,0,0, name);
}

int GCodeInterpreter::SetActionProgramPC(int action_num, const char * name)
{
    return setAction(action_num, M_Action_Program_PC, 0,0,0,0,0, name);
}

int GCodeInterpreter::SetActionCallback(int action_num)
{
    return setAction(action_num, M_Action_Callback);
}

int GCodeInterpreter::SetActionWaitbit(int action_num, int bit, int state)
{
    return setAction(action_num, M_Action_Waitbit, bit, state);
}


/*
    Since the GCode interpreter runs as a separate thread, the following _gci_* callbacks
    will be invoked from a thread created outside the binding language.
    E.g. for Python, the GIL must be handled correctly.
    
    The following macros should be defined as appropriate for each binding language
*/
#ifdef SWIGPYTHON
    #define _ENTER_BINDING  PyGILState_STATE gstate = PyGILState_Ensure()
    #define _EXIT_BINDING  PyGILState_Release(gstate)
    #define _INIT_THREADS PyEval_InitThreads()
#else
    #error "You'll need to define _ENTER_BINDING / _EXIT_BINDING / _INIT_THREADS appropriately for this language."
#endif


static void _gci_status(int line_no, const char *msg)
{
    //printf("-status- %d %s\n", line_no, msg);
    if (_theInterpreter) {
        _ENTER_BINDING;
        _theInterpreter->HandleStatus(line_no, msg);
        _EXIT_BINDING;
    }
}

static void _gci_complete(int status, int lineno, int sequence_number, const char *err)
{
    //printf("-compleat- %d %d %d %s\n", status, lineno, sequence_number, err);
    if (_theInterpreter) {
        _ENTER_BINDING;
        _theInterpreter->HandleComplete(status, lineno, sequence_number, err);
        _EXIT_BINDING;
    }
}

static int _gci_user(const char *msg)
{
    int rc = 0;
    //printf("-user- %s\n", msg);
    if (_theInterpreter) {
        _ENTER_BINDING;
        rc = _theInterpreter->HandleUser(msg);
        _EXIT_BINDING;
    }
    return rc;
}

static int _gci_mcode(int mCode)
{
    int rc = 0;
    //printf("-mcode- %d\n", mCode);
    if (_theInterpreter) {
        _ENTER_BINDING;
        rc = _theInterpreter->HandleMCode(mCode);
        _EXIT_BINDING;
    }
    return rc;
}

GCodeInterpreter::GCodeInterpreter(CCoordMotion *CM): CGCodeInterpreter(CM) 
{
    _theInterpreter = this;
    CGCodeInterpreter::SetUserCallback(_gci_user);
    CGCodeInterpreter::SetUserMCodeCallback(_gci_mcode);
    _INIT_THREADS;
}

int GCodeInterpreter::Interpret(
				  int board_type,
		          const char *fname,
			      int start, int end,
				  int restart)
{
    return CGCodeInterpreter::Interpret(board_type, fname, start, end, restart, _gci_status, _gci_complete);
}



