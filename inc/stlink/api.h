/* -----------------------------------------------
 * Created by Andreas Michelis on 2024-09-19.
 * Copyright (c) 2024, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef STLINK_API_H
#define STLINK_API_H

#ifdef _WIN32
#   if defined STLINK_EXPORT
#       define STLINK_API __declspec(dllexport)
#   elif defined STLINK_STATIC
#       define STLINK_API
#   else // STLINK_EXPORT || STLINK_STATIC
#       define STLINK_API __declspec(dllimport)
#   endif // STLINK_EXPORT || STLINK_STATIC
#else // _WIN32
#   define STLINK_API
#endif // _WIN32


#endif //STLINK_API_H
