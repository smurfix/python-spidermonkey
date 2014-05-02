// Special helpers to simplify program structure and error handling
// in C++ Python code.

template <typename T>
class CPyAuto
{
  public:
    CPyAuto(T *pyo) {
	m_pyo = pyo;
    }
    ~CPyAuto() {
	Py_XDECREF(m_pyo);
    }

    T * operator->() const { return m_pyo; }
    operator T*() { return m_pyo; }

    // Steal the object from this auto, usually used when returning a New reference from a
    // context where it needed temporary protection from errors.

    T * asNew() { 
	T *ret = m_pyo;
	m_pyo = NULL;
	return ret;
    }

    void setFromNew(T *newobj) {
	Py_XDECREF(m_pyo);
	m_pyo = newobj;
    }

    void setFromBorrowed(T *newobj) {
	*this = newobj;
	if (newobj != NULL)
	    Py_INCREF(newobj);
    }

    CPyAuto& operator=(T *newobj) {
	this->setFromNew(newobj);
	return *this;
    }

    bool isNull() { return m_pyo == NULL; }

  protected:
    T *m_pyo;
};


typedef CPyAuto<PyObject> CPyAutoObject;
 
template <typename T>
class CPyAutoFree
{
  public:
    CPyAutoFree(T *ptr) { m_ptr = ptr; }
    ~CPyAutoFree() { if (m_ptr) free(m_ptr); }

    T* operator->() const { return m_ptr; }
    operator T*() { return m_ptr; }

    T* steal() { 
	T *ret = m_ptr;
	m_ptr = NULL;
	return ret;
    }

    CPyAutoFree& operator=(T* newobj) {
	if (m_ptr) free(m_ptr);
	m_ptr = newobj;
	return *this;
    }

    bool isNull() { return m_ptr == NULL; }

  protected:
    T *m_ptr;
};

typedef CPyAutoFree<JSClass> CPyAutoFreeJSClassPtr;
typedef CPyAutoFree<char> CPyAutoFreeCharPtr;
