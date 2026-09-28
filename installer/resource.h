#pragma once

// IDC_STATIC comes from the MFC headers, which this project does not use, so
// it is defined here instead.
#ifndef IDC_STATIC
#define IDC_STATIC     (-1)
#endif

// Installer resources. Kept in their own range so an ID here can never clash
// with anything the application defines.
#define IDI_APP        201
#define IDD_SETUP      202

#define IDC_TITLE      2001
#define IDC_SUBTITLE   2002
#define IDC_DESTPATH   2003
#define IDC_DESKTOP    2004
#define IDC_STATUS     2005
#define IDC_PROGRESS   2006
