#include "Python.h"

//#include <stropts.h> // ioctl()
////#include <netinet/in.h>
//#include <net/if.h>
//#include <linux/sockios.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <stdint.h>
////#include <bits/socket.h>
////#include <bits/sockaddr.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netdevice.h>

//#include <linux/if.h>
#define IF_NAMESIZE 16

static int sockios_fd = 0;

static PyObject *SockiosError;


// C API

struct sockios_ifc {
	const char ifname[IF_NAMESIZE];
	uint8_t  in_addr[4];
	uint8_t  hw_addr[6];
	int16_t  flags;
	uint16_t hw_family;
	uint16_t in_family;
};

static int
PySockios_IfFlags(const char *ifname, int *flags) {
	struct ifreq ifr;
	int err;
	
	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE-1);
	
	if ((err = ioctl(sockios_fd, SIOCGIFFLAGS, &ifr)) < 0) {
		return err;
	}

	*flags = ifr.ifr_flags;

	return err;
}

static int
PySockios_IfAddr(const char *ifname, int *family, uint8_t *addr) {
	struct ifreq ifr;
	int err;
	
	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
	
	if ((err = ioctl(sockios_fd, SIOCGIFADDR, &ifr)) < 0) {
		return err;
	}
	
	if (addr) {
		/* skip port number */
		memcpy(addr, ifr.ifr_addr.sa_data+2, 4); 
	}
	
	if (family) {
		*family = ifr.ifr_addr.sa_family;
	}

	return err;
}

static int
PySockios_IfAddrStr(const char *ifname, int *family, char *addr) {
	uint8_t in_addr[4] = {};
	int err;

	if ((err = PySockios_IfAddr(ifname, family, in_addr)) < 0) {
		return err;
	}

	if (addr) {
		snprintf(addr, 16, "%i.%i.%i.%i",
			in_addr[0], in_addr[1],
			in_addr[1], in_addr[2]);
	}
	
	return err;
}

static int
PySockios_IfHwAddr(const char *ifname, int *family, uint8_t *addr) {
	struct ifreq ifr;
	int err;
	
	strncpy(ifr.ifr_name, ifname, IF_NAMESIZE);
	
	if ((err = ioctl(sockios_fd, SIOCGIFHWADDR, &ifr)) < 0) {
		return err;
	}

	if (addr) {
		memcpy(addr, ifr.ifr_addr.sa_data, 6);
	}

	if (family) {
		*family = ifr.ifr_hwaddr.sa_family;
	}
	
	return err;
}

static int
PySockios_IfHwAddrStr(const char *ifname, int *family, char *addr) {
	uint8_t hw_addr[6] = {};
	int err;

	if ((err = PySockios_IfHwAddr(ifname, family, hw_addr)) < 0) {
		return err;
	}

	if (addr) {
		snprintf(addr, 18, "%0x:%0x:%0x:%0x:%0x:%0x",
			hw_addr[0], hw_addr[1], hw_addr[2],
			hw_addr[3], hw_addr[4], hw_addr[5]);
	}

	return err;
}

// Python API

static PyObject *
sockios_init(PyObject *self, PyObject *args)
{
	if ((sockios_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}
	return Py_None;
}

static PyObject *
sockios_is_up(PyObject *self, PyObject *args)
{
	const char *ifname;
	int flags;

	if (!PyArg_ParseTuple(args, "s", &ifname))
		return NULL;

	if (PySockios_IfFlags(ifname, &flags) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}

	return Py_BuildValue("i", (
				(flags & IFF_UP) &&
				(flags & IFF_RUNNING)
				));
}

static PyObject *
sockios_get_ifconf(PyObject *self, PyObject *args)
{
	const char *ifname;
	char in_addr[16], hw_addr[18];
	int flags, in_family, hw_family;

	if (!PyArg_ParseTuple(args, "s", &ifname))
		return NULL;

	if (PySockios_IfFlags(ifname, &flags) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}

	if (PySockios_IfAddrStr(ifname, &in_family, in_addr) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}

	if (PySockios_IfHwAddrStr(ifname, &hw_family, hw_addr) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}

	return Py_BuildValue("{s:i,s:s,s:i,s:s,s:i}",
		"flags", flags,
		"in_addr", in_addr,
		"in_family", in_family,
		"hw_addr", hw_addr,
		"hw_family", hw_family);
}

static PyObject *
sockios_get_iflist(PyObject *self, PyObject *args)
{
	char buff[1024];
	int i;
	struct ifconf ifc;
	struct ifreq *ifr;

	PyObject *result, *entry;

	if (!PyArg_ParseTuple(args, ""))
		return NULL;
	
	ifc.ifc_len = sizeof(buff);
	ifc.ifc_buf = buff;

	if (ioctl(sockios_fd, SIOCGIFCONF, &ifc) < 0) {
		return PyErr_SetFromErrno(SockiosError);
	}

	ifr = ifc.ifc_req;

	result = PyList_New(0);

	for (i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++) {
		entry = Py_BuildValue("s", ifr->ifr_name);

		if (PySequence_Contains(result, entry) == 0) {
			if (PyList_Append(result, entry) < 0) {
				Py_DECREF(result);
				return NULL;
			}
		}

		Py_DECREF(entry);
	}


	return result;
}


static PyMethodDef SockioMethods[] = {
    {"init",  sockios_init, METH_VARARGS,
	    "Initialize the sockios module."},
    {"get_iflist",  sockios_get_iflist, METH_VARARGS,
	    "Returns a list of interfaces available on the system."},
    {"get_ifconf",  sockios_get_ifconf, METH_VARARGS,
	    "Obtains information about an interface."},
    {"is_up",  sockios_is_up, METH_VARARGS,
	    "Returns True if the interface is up."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyAPI_FUNC(PyObject *) Py_InitModule4(const char *name, PyMethodDef *methods,
                                      const char *doc, PyObject *self,
                                      int apiver);
PyMODINIT_FUNC
initsockios(void)
{
	PyObject *m = Py_InitModule4("sockios", SockioMethods,
			NULL, NULL,
			PYTHON_API_VERSION);
	
	if (m == NULL)
		return;

	SockiosError = PyErr_NewException("sockios.error", PyExc_OSError, NULL);
	Py_INCREF(SockiosError);
	PyModule_AddObject(m, "error", SockiosError);
}