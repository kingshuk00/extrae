# AX_FLAGS_SAVE
# -------------
AC_DEFUN([AX_FLAGS_SAVE],
[
   saved_LIBS="${LIBS}"
   saved_CC="${CC}"
   saved_CFLAGS="${CFLAGS}"
   saved_CXXFLAGS="${CXXFLAGS}"
   saved_CPPFLAGS="${CPPFLAGS}"
   saved_LDFLAGS="${LDFLAGS}"
])


# AX_FLAGS_RESTORE
# ----------------
AC_DEFUN([AX_FLAGS_RESTORE],
[
   LIBS="${saved_LIBS}"
   CC="${saved_CC}"
   CFLAGS="${saved_CFLAGS}"
   CXXFLAGS="${saved_CXXFLAGS}"
   CPPFLAGS="${saved_CPPFLAGS}"
   LDFLAGS="${saved_LDFLAGS}"
])

# AX_FIND_INSTALLATION
#   (package, [list-of-possible-root-paths], 
#    [list-of-required-bins], [list-of-optional-bins],
#    [list-of-required-headers], [list-of-optional-headers],
#    [list-of-required-libs], [list-of-optional-libs],
#    [action-if-yes], [action-if-no])
# ----------------------------------------------------------
AC_DEFUN([AX_FIND_INSTALLATION],
[
  AC_REQUIRE([AX_SELECT_BINARY_TYPE])

dnl AX_FLAGS_SAVE()

  dnl Generate lowercase and uppercase tokens for the package name (e.g. MPI), later used
  dnl to add --with flags (e.g. --with-mpi-libs) and to declare the output variables (e.g. MPI_HOME)
  m4_define([__pkg_name_lcase], m4_tolower($1))
  m4_define([__pkg_name_ucase], m4_toupper($1))

  dnl Search for home directory
  AC_MSG_CHECKING([for __pkg_name_ucase installation])
  for home_dir in [$2 "not found"]; do
    if test -d "${home_dir}/${BITS}" ; then
      home_dir="${home_dir}/${BITS}"
      break
    elif test -d "${home_dir}" ; then
      break
    fi
  done

  AC_MSG_RESULT([${home_dir}])
  m4_define([__PKG_HOME], __pkg_name_ucase[]_HOME) dnl Output variable <PKG>_HOME
  __PKG_HOME=${home_dir}
  AC_SUBST(__PKG_HOME)

  dnl Control rpath setting per dependency
  AC_ARG_ENABLE(__pkg_name_lcase[]-rpath,
    AC_HELP_STRING(
      [--disable-[]__pkg_name_lcase[]-rpath],
      [Embed rpath for __pkg_name_ucase dependencies (enabled by default)]
    ),
    [enable_pkg_rpath="${enableval}"],
    [enable_pkg_rpath="yes"]
  )

  dnl Did the user pass a bin directory to check first?
  AC_ARG_WITH([__pkg_name_lcase[]-binaries],
    AC_HELP_STRING(
      [--with-[]__pkg_name_lcase[]-binaries@<:@=DIR@:>@],
      [Specify location of binary files for package __pkg_name_ucase]
    ),
    [ForcedBins="${withval}"],
    [ForcedBins=""]
  )

  dnl Search for binaries directory
  AC_MSG_CHECKING([for __pkg_name_ucase binaries directory])
  for bins_dir in [${ForcedBins} ${home_dir}/bin${BITS} ${home_dir}/bin "not found"] ; do
    if test -d "${bins_dir}" ; then
      break
    fi
  done
  AC_MSG_RESULT([${bins_dir}])

  m4_define([__PKG_BINSDIR], __pkg_name_ucase[]_BINSDIR) dnl Output variable <PKG>_BINSDIR
  __PKG_BINSDIR=${bins_dir}
  AC_SUBST(__PKG_BINSDIR)

  if test "${bins_dir}" != "not found"; then
    bin_folder_found="yes"
  else
    bin_folder_found="no"
  fi

  dnl Search for binaries
  required_bins_found="yes"
  if test "${bin_folder_found}" = "yes"; then
    :

    dnl Search for required binaries
    m4_foreach_w([bin_name], [$3], [
      AC_MSG_CHECKING([for __pkg_name_ucase required binary bin_name])
      if test -x "${bins_dir}/bin_name" ; then
        bin_path="${bins_dir}/bin_name"

        dnl Make variable named <PKG>_BIN_<binary> (e.g. MPI_BIN_mpirun)
        m4_pushdef([__PKG_BINARY_PATH], m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _))
        dnl Assign value to this variable (e.g. MPI_BIN_mpirun=<path-to-mpirun>)
        __PKG_BINARY_PATH=${bin_path}
        dnl Make it available in the Makefiles
        AC_SUBST(__PKG_BINARY_PATH)
        m4_popdef([__PKG_BINARY_PATH])
        AC_MSG_RESULT([${bins_dir}/bin_name])
      else
        AC_MSG_RESULT([no])
        required_bins_found="no"
      fi
    ])

    dnl Search for optional binaries
    m4_foreach_w([bin_name], [$4], [
      AC_MSG_CHECKING([for __pkg_name_ucase optional binary bin_name])

      if test -x "${bins_dir}/bin_name" ; then
        bin_path="${bins_dir}/bin_name"

        dnl Make variable named <PKG>_BIN_<binary> (e.g. MPI_BIN_mpirun)
        m4_pushdef([__PKG_BINARY_PATH], m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _))
        dnl Assign value to this variable (e.g. MPI_BIN_mpirun=<path-to-mpirun>)
        __PKG_BINARY_PATH=${bin_path}
        dnl Make it available in the Makefiles
        AC_SUBST(__PKG_BINARY_PATH)
        m4_popdef([__PKG_BINARY_PATH])
        AC_MSG_RESULT([${bins_dir}/bin_name])
      else
        AC_MSG_RESULT([no])
      fi
    ])
  fi

  dnl Did the user pass a headers directory to check first?
  AC_ARG_WITH([__pkg_name_lcase[]-headers],
    AC_HELP_STRING(
      [--with-[]__pkg_name_lcase[]-headers@<:@=DIR@:>@],
      [Specify location of include files for package __pkg_name_ucase]
    ),
    [ForcedHeaders="${withval}"],
    [ForcedHeaders=""]
  )

  dnl Search for includes directory
  AC_MSG_CHECKING([for __pkg_name_ucase includes directory])
  for incs_dir in [${ForcedHeaders} ${home_dir}/include${BITS} ${home_dir}/include "not found"] ; do
    if test -d "${incs_dir}" ; then
      break
    fi
  done
  AC_MSG_RESULT([${incs_dir}])

  m4_define([__PKG_INCSDIR], __pkg_name_ucase[]_INCSDIR) dnl Output variable <PKG>_INCSDIR
  __PKG_INCSDIR=${incs_dir}
  AC_SUBST(__PKG_INCSDIR)

  m4_define([__PKG_INCLUDES], __pkg_name_ucase[]_INCLUDES) dnl Output variable <PKG>_INCLUDES (alias of <PKG>_INCSDIR, deprecated)
  __PKG_INCLUDES=${incs_dir}
  AC_SUBST(__PKG_INCLUDES)

  if test "${incs_dir}" != "not found"; then
    include_folder_found="yes"
  else
    include_folder_found="no"
  fi

  dnl Search for includes
  required_headers_found="yes"
  if test "${include_folder_found}" = "yes" ; then
    m4_define([__PKG_CFLAGS], __pkg_name_ucase[]_CFLAGS) dnl Output variable <PKG>_CFLAGS
    __PKG_CFLAGS="-I${incs_dir}"
    m4_define([__PKG_CXXFLAGS], __pkg_name_ucase[]_CXXFLAGS) dnl Output variable <PKG>_CXXFLAGS
    __PKG_CXXFLAGS="-I${incs_dir}"
    m4_define([__PKG_CPPFLAGS], __pkg_name_ucase[]_CPPFLAGS) dnl Output variable <PKG>_CPPFLAGS
    __PKG_CPPFLAGS="-I${incs_dir}"

    if test ! -z "${multiarch_triplet}" ; then
      if test -d "${incs_dir}/${multiarch_triplet}" ; then
        __PKG_CFLAGS="${__PKG_CFLAGS} -I${incs_dir}/${multiarch_triplet}"
        __PKG_CXXFLAGS="${__PKG_CXXFLAGS} -I${incs_dir}/${multiarch_triplet}"
        __PKG_CPPFLAGS="${__PKG_CPPFLAGS} -I${incs_dir}/${multiarch_triplet}"
      fi
    fi

    AC_SUBST(__PKG_CFLAGS)
    AC_SUBST(__PKG_CXXFLAGS)
    AC_SUBST(__PKG_CPPFLAGS)

    dnl Update the default variables so the automatic checks will take into account the new directories
    CFLAGS="${CFLAGS} ${__PKG_CFLAGS}"
    CXXFLAGS="${CXXFLAGS} ${__PKG_CXXFLAGS}"
    CPPFLAGS="${CPPFLAGS} ${__PKG_CPPFLAGS}"

    dnl Test for required headers
    if test -n "$5"; then
      AC_CHECK_HEADERS($5,
        [ required_headers_found="yes" ],
        [ required_headers_found="no" ])
    fi

    dnl Test for optional headers
    if test -n "$6"; then
      AC_CHECK_HEADERS($6,
        [ optional_headers_found="yes" ],
        [ optional_headers_found="no" ])
    fi
  fi

  dnl Did the user pass a libraries directory to check first?
  AC_ARG_WITH([__pkg_name_lcase[]-libs],
    AC_HELP_STRING(
      [--with-[]__pkg_name_lcase[]-libs@<:@=DIR@:>@],
      [Specify location of library files for package __pkg_name_ucase]
    ),
    [ForcedLibs="${withval}"],
    [ForcedLibs=""]
  )

  dnl Search for libs directory
  AC_MSG_CHECKING([for __pkg_name_ucase libraries directory])
  for libs_dir in [${ForcedLibs} ${home_dir}/lib${BITS} ${home_dir}/lib "not found"] ; do
    if test -d "${libs_dir}" ; then
      break
    fi
  done
  AC_MSG_RESULT([${libs_dir}])

  m4_define([__PKG_LIBSDIR], __pkg_name_ucase[]_LIBSDIR) dnl Output variable <PKG>_LIBSDIR
  __PKG_LIBSDIR=${libs_dir}
  AC_SUBST(__PKG_LIBSDIR)

  if test "${libs_dir}" != "not found"; then
    lib_folder_found="yes"
  else
    lib_folder_found="no"
  fi

  dnl Search for extra libs directories
  if test "${lib_folder_found}" = "yes" ; then
    m4_define([__PKG_LDFLAGS], __pkg_name_ucase[]_LDFLAGS) dnl Output variable <PKG>_LDFLAGS
    __PKG_LDFLAGS="-L${libs_dir}"
    m4_define([__PKG_RPATH], __pkg_name_ucase[]_RPATH) dnl Output variable <PKG>_RPATH
    __PKG_RPATH="-R${libs_dir}"

    AC_MSG_CHECKING([for __pkg_name_ucase shared library folder])
    m4_define([__PKG_LIBSDIR_SHARED], __pkg_name_ucase[]_LIBSDIR_SHARED) dnl Output variable <PKG>_LIBSDIR_SHARED
    __PKG_LIBSDIR_SHARED=""
    if test -d "${libs_dir}/shared" ; then
      __PKG_LIBSDIR_SHARED="${libs_dir}/shared" 
      __PKG_LDFLAGS="-L${__PKG_LIBSDIR_SHARED} ${__PKG_LDFLAGS}"
      __PKG_RPATH="-R${__PKG_LIBSDIR_SHARED} ${__PKG_RPATH}"
      AC_MSG_RESULT([${__PKG_LIBSDIR_SHARED}])
    else
      dnl Added for retrocompatibility
      __PKG_LIBSDIR_SHARED="${libs_dir}" 
      AC_MSG_RESULT([no])
    fi
    AC_SUBST(__PKG_LIBSDIR_SHARED)

    AC_MSG_CHECKING([for __pkg_name_ucase multiarch library folder])
    m4_define([__PKG_LIBSDIR_MULTIARCH], __pkg_name_ucase[]_LIBSDIR_MULTIARCH) dnl Output variable <PKG>_LIBSDIR_MULTIARCH
    __PKG_LIBSDIR_MULTIARCH=""
    if test -n "${multiarch_triplet}" -a -d "${libs_dir}/${multiarch_triplet}" ; then
      __PKG_LIBSDIR_MULTIARCH="${libs_dir}/${multiarch_triplet}" 
      __PKG_LDFLAGS="-L${__PKG_LIBSDIR_MULTIARCH} ${__PKG_LDFLAGS}"
      __PKG_RPATH="-R${__PKG_LIBSDIR_MULTIARCH} ${__PKG_RPATH}"
      AC_MSG_RESULT([${__PKG_LIBSDIR_MULTIARCH}])
    else
      dnl Added for retrocompatibility
      __PKG_LIBSDIR_MULTIARCH="${libs_dir}" 
      AC_MSG_RESULT([no])
    fi
    AC_SUBST(__PKG_LIBSDIR_MULTIARCH)

    if test "${enable_rpath}" = "no" -o "${enable_pkg_rpath}" = "no"; then
      __PKG_RPATH=""
    fi
    AC_SUBST(__PKG_RPATH)
    
    m4_define([__PKG_LDFLAGS_RPATH], __pkg_name_ucase[]_LDFLAGS_RPATH) dnl Output variable <PKG>_LDFLAGS_RPATH
    __PKG_LDFLAGS_RPATH="${__PKG_LDFLAGS} ${__PKG_RPATH}"

    dnl Search for required libraries in any of the possible folders
    required_libs_found="yes"
    all_libs_dirs="${__PKG_LIBSDIR_MULTIARCH} ${__PKG_LIBSDIR_SHARED} ${__PKG_LIBSDIR}"

    m4_define([__PKG_LIBS], __pkg_name_ucase[]_LIBS) dnl Output variable <PKG>_LIBS
    __PKG_LIBS=""

    m4_foreach_w([lib_name], [$7], [
      AC_MSG_CHECKING([for __pkg_name_ucase required library lib[]lib_name])

      found=0
      for lib_path in $all_libs_dirs; do
        for lib_extension in .so .dylib .a; do
          if test -f "${lib_path}/lib[]lib_name[]${lib_extension}" ; then
            AC_MSG_RESULT([${lib_path}/lib[]lib_name[]${lib_extension}])
            __PKG_LIBS+="-l[]lib_name "
            found=1

            m4_pushdef([__PKG_LIBRARY_PATH], __pkg_name_ucase[]_LIB_[]lib_name) dnl Output variable <PKG>_LIB_<library> (e.g. MPI_LIB_mpich)
            __PKG_LIBRARY_PATH=${lib_path}/lib[]lib_name[]${lib_extension}
            AC_SUBST(__PKG_LIBRARY_PATH)
            m4_popdef([__PKG_LIBRARY_PATH])

            break 2
          fi
        done
      done
      if (( $found == 0 )); then
        AC_MSG_RESULT([no])
        required_libs_found="no"
      fi
    ])

    dnl Search for optional libraries in any of the possible folders
    m4_foreach_w([lib_name], [$8], [
      AC_MSG_CHECKING([for __pkg_name_ucase optional library lib[]lib_name])

      found=0
      for lib_path in $all_libs_dirs; do
        for lib_extension in .so .dylib .a; do
          if test -f "${lib_path}/lib[]lib_name[]${lib_extension}" ; then
            AC_MSG_RESULT([${lib_path}/lib[]lib_name[]${lib_extension}])
            __PKG_LIBS+="-l[]lib_name "
            found=1

            m4_pushdef([__PKG_LIBRARY_PATH], __pkg_name_ucase[]_LIB_[]lib_name) dnl Output variable <PKG>_LIB_<library> (e.g. MPI_LIB_mpich)
            __PKG_LIBRARY_PATH=${lib_path}/lib[]lib_name[]${lib_extension}
            AC_SUBST(__PKG_LIBRARY_PATH)
            m4_popdef([__PKG_LIBRARY_PATH])

            break 2
          fi
        done 
      done
      if (( $found == 0 )); then
        AC_MSG_RESULT([no])
      fi
    ])

    __PKG_LDFLAGS="${__PKG_LDFLAGS} ${__PKG_LIBS}"
    AC_SUBST(__PKG_LDFLAGS)
    __PKG_LDFLAGS_RPATH="${__PKG_LDFLAGS_RPATH} ${__PKG_LIBS}"
    AC_SUBST(__PKG_LDFLAGS_RPATH)
    AC_SUBST(__PKG_LIBS)

    dnl Update the default variables so the automatic checks will take into account the new directories
    LDFLAGS="${LDFLAGS} ${__PKG_LDFLAGS}"
  fi

  dnl Add defines and Makefile conditionals for the detected binaries and libraries
  m4_foreach_w([bin_name], [$3], [
    if test "${m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _)}" != ""; then
      AC_DEFINE([HAVE_[]m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _)], 1, "Define to 1 if __pkg_name_ucase binary bin_name is found")
    fi
#    AM_CONDITIONAL(HAVE_[]m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _), test "${__pkg_name_ucase[]_BIN_[]bin_name}" != "")
  ])
  m4_foreach_w([bin_name], [$4], [
    if test "${m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _)}" != ""; then
      AC_DEFINE([HAVE_[]m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _)], 1, "Define to 1 if __pkg_name_ucase binary bin_name is found")
    fi
#    AM_CONDITIONAL(HAVE_[]m4_bpatsubst(__pkg_name_ucase[]_BIN_[]bin_name, -, _), test "${__pkg_name_ucase[]_BIN_[]bin_name}" != "")
  ])
  m4_foreach_w([lib_name], [$7], [
    if test "${__pkg_name_ucase[]_LIB_[]lib_name}" != ""; then
      AC_DEFINE([HAVE_[]__pkg_name_ucase[]_LIB_[]lib_name], 1, "Define to 1 if __pkg_name_ucase library lib[]lib_name is found")
    fi
#    AM_CONDITIONAL(HAVE_[]__pkg_name_ucase[]_LIB_[]lib_name, test "${__pkg_name_ucase[]_LIB_[]lib_name}" != "")
  ])
  m4_foreach_w([lib_name], [$8], [
    if test "${__pkg_name_ucase[]_LIB_[]lib_name}" != ""; then
      AC_DEFINE([HAVE_[]__pkg_name_ucase[]_LIB_[]lib_name], 1, "Define to 1 if __pkg_name_ucase library lib[]lib_name is found")
    fi
#    AM_CONDITIONAL(HAVE_[]__pkg_name_ucase[]_LIB_[]lib_name, test "${__pkg_name_ucase[]_LIB_[]lib_name}" != "")
  ])

  dnl Everything went OK? (maybe we should tweak these checks a little bit)
  AC_MSG_CHECKING([for __pkg_name_ucase valid installation])

  m4_define([__PKG_INSTALLED], __pkg_name_ucase[]_INSTALLED) dnl Output variable <PKG>_INSTALLED
  __PKG_INSTALLED="yes"

  if test "${__PKG_HOME}" = "not found"; then
    __PKG_INSTALLED="no"
  fi

  if test "$3" != "" -a "${bin_folder_found}" = "no" ; then
    __PKG_INSTALLED="no"
  fi

  if test "${required_bins_found}" = "no"; then
    __PKG_INSTALLED="no"
  fi

  if test "$5" != "" -a "${include_folder_found}" = "no" ; then
    __PKG_INSTALLED="no"
  fi

  if test "${required_headers_found}" = "no"; then
    __PKG_INSTALLED="no"
  fi

  if test "$7" != "" -a "${lib_folder_found}" = "no" ; then
    __PKG_INSTALLED="no"
  fi

  if test "${required_libs_found}" = "no" ; then
    __PKG_INSTALLED="no"
  fi

  dnl Run the action-if-yes / action-if-no
  if test "${__PKG_INSTALLED}" = "yes" ; then
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_[]__pkg_name_ucase, [1], "Define when valid __pkg_name_ucase installation found")
    $9
  else
    AC_MSG_RESULT([no])
    $10
  fi

  dnl AX_FLAGS_RESTORE()
])

# AX_SELECT_BINARY_TYPE
# ---------------------
# Check the binary type the user wants to build and verify whether it can be successfully built
AC_DEFUN([AX_SELECT_BINARY_TYPE],
[
	AC_CHECK_SIZEOF([void *])

	AC_ARG_WITH(binary-type,
		AC_HELP_STRING(
			[--with-binary-type@<:@=ARG@:>@],
			[choose the binary type between: 32, 64, default @<:@default=default@:>@]
		),
		[Selected_Binary_Type="$withval"],
		[Selected_Binary_Type="default"]
	)

	if test "$Selected_Binary_Type" != "default" -a "$Selected_Binary_Type" != "32" -a "$Selected_Binary_Type" != "64" ; then
		AC_MSG_ERROR([--with-binary-type: Invalid argument '$Selected_Binary_Type'. Valid options are: 32, 64, default.])
	fi

	C_compiler="$CC"
	CXX_compiler="$CXX"

	AC_LANG_SAVE([])
	m4_foreach([language], [[C], [C++]], [
		AC_LANG_PUSH(language)

		AC_CACHE_CHECK(
			[for $_AC_LANG_PREFIX[]_compiler compiler default binary type], 
			[[]_AC_LANG_PREFIX[]_ac_cv_compiler_default_binary_type],
			[
				Default_Binary_Type=$(( 8 * ${ac_cv_sizeof_void_p} ))
				[]_AC_LANG_PREFIX[]_ac_cv_compiler_default_binary_type="$Default_Binary_Type""-bit"
			]
		)

		which dpkg-architecture &> /dev/null
		if test "$?" -eq "0"; then
			if test "${Selected_Binary_Type}" = "default" ; then
				AC_MSG_CHECKING([the multiarch triplet through dpkg-architecture])
				multiarch_triplet=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
				AC_MSG_RESULT([$multiarch_triplet])
			fi
		else
			AC_MSG_NOTICE([cannot locate multiarch triplet])
		fi

		if test "$Default_Binary_Type" != "32" -a "$Default_Binary_Type" != 64 ; then
			[]_AC_LANG_PREFIX[]_PRESENT="no"
			msg="language compiler '$_AC_LANG_PREFIX[]_compiler' seems not to be installed in the system.  Please verify there is a working installation of the language compiler '$_AC_LANG_PREFIX[]_compiler'."
			if test "language" == "C" ; then
				AC_MSG_ERROR($msg)
			else 
				AC_MSG_WARN($msg)
			fi
		else
			[]_AC_LANG_PREFIX[]_PRESENT="yes"
		fi

		if test "$Selected_Binary_Type" = "default" ; then
			Selected_Binary_Type="$Default_Binary_Type"
		fi

		if test "$Selected_Binary_Type" != "$Default_Binary_Type" -a "$[]_AC_LANG_PREFIX[]_PRESENT" = "yes" ; then

			force_bit_flags="-m32 -q32 -32 -maix32 -m64 -q64 -64 -maix64 none"

			AC_MSG_CHECKING([for $_AC_LANG_PREFIX[]_compiler compiler flags to build a $Selected_Binary_Type-bit binary])
			for flag in [$force_bit_flags]; do
				old_[]_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS"
				[]_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $flag"

				if test $(( 8 * ${ac_cv_sizeof_void_p})) = "${Selected_Binary_Type}" ; then
					AC_MSG_RESULT([$flag])
					break
				else
					[]_AC_LANG_PREFIX[]FLAGS="$old_[]_AC_LANG_PREFIX[]FLAGS"
					if test "$flag" = "none" ; then
						AC_MSG_RESULT([unknown])
						AC_MSG_NOTICE([${Selected_Binary_Type}-bit binaries not supported])
						AC_MSG_ERROR([Please use '--with-binary-type' to select an appropriate binary type.])

					fi
				fi
			done

		fi
		AC_LANG_POP(language)
	])
	AC_LANG_RESTORE([])
	BITS="$Selected_Binary_Type"
])

# AX_ENSURE_CXX_PRESENT
# ---------------------
# Check the cxx compiler is present
AC_DEFUN([AX_ENSURE_CXX_PRESENT],
[
  AC_REQUIRE([AX_SELECT_BINARY_TYPE])

  if test "$CXX_PRESENT" != "yes" ; then
    AC_MSG_ERROR([You have enabled the '$1' functionality which requires a working CXX compiler installed in the system, but it seems the compiler is not present. Check the 'config.log' file for previous errors or disable this option.])
  fi
])


# AX_CHECK_ENDIANNESS
# -------------------
# Test if the architecture is little or big endian
AC_DEFUN([AX_CHECK_ENDIANNESS],
[
   AC_C_BIGENDIAN(
      AC_DEFINE(IS_BIG_ENDIAN, 1, [Define to 1 if architecture is big endian]),
      AC_DEFINE(IS_LITTLE_ENDIAN, 1, [Define to 1 if architecture is little endian]),
      AC_MSG_FAILURE([Cannot detect endianness]))
])


# AX_CHECK__FUNCTION__MACRO
# -------------------------
# Check whether the compiler defines the __FUNCTION__ macro
AC_DEFUN([AX_CHECK__FUNCTION__MACRO],
[
   AC_CACHE_CHECK([whether the compiler defines the __FUNCTION__ macro], [ac_cv_have__function__],
      [
         AC_LANG_SAVE()
         AC_LANG([C])
         AC_TRY_COMPILE(
            [#include <stdio.h>],
            [
               char *s = __FUNCTION__;
               return 0;
            ],
            [ac_cv_have__function__="yes"],
            [ac_cv_have__function__="no"]
         )
         AC_LANG_RESTORE()
      ]
   )
   if test "$ac_cv_have__function__" = "yes" ; then
      AC_DEFINE([HAVE__FUNCTION__], 1, [Define to 1 if __FUNCTION__ macro is supported])
   fi
])

AC_DEFUN([AX_CHECK_PGI],
[
   AC_MSG_CHECKING(for PGI C compiler)
   AX_FLAGS_SAVE()
   AC_LANG_SAVE()
   AC_LANG([C])
   AC_TRY_COMPILE(
      [],
      [
         #if !defined (__PGI__) && !defined(__PGI)
         # error "This is for PGI compilers only"
         #endif
         return 0;
      ],
      [pgi_compiler="yes"],
      [pgi_compiler="no"]
   )
   AX_FLAGS_RESTORE()
   AC_LANG_RESTORE()
   if test "${pgi_compiler}" = "yes"; then
      AC_MSG_RESULT([yes])
   else
      AC_MSG_RESULT([no])
   fi
])

# AX_PROG_XML2
# -----------
# Check for libxml2 installation
AC_DEFUN([AX_PROG_XML2],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(xml-prefix,
      AC_HELP_STRING(
         [--with-xml-prefix@<:@=DIR@:>@],
         [specify where to find libxml2 libraries and includes (deprecated, use --with-xml)]
      ),
      [xml_paths="${withval}"],
      [xml_paths="/usr/local /usr"] dnl List of possible default paths
   )

   AC_ARG_WITH(xml,
      AC_HELP_STRING(
         [--with-xml@<:@=DIR@:>@],
         [specify where to find libxml2 libraries and includes]
      ),
      [xml_paths="${withval}"]
      dnl [xml_paths="/usr/local /usr"] dnl List of possible default not given here, taken from --with-xml-prefix
      dnl                               dnl Activate this again when --with-xml-prefix is removed
   )

   AX_FIND_INSTALLATION([XML2], [${xml_paths}], [xml2-config], [], [], [], [xml2], [], [], [])
   if test "$XML2_INSTALLED" = "yes" ; then

      min_xml_version=ifelse([$1], ,2.0.0, [$1])
      AC_MSG_CHECKING(for libxml2 version >= $min_xml_version)
      min_xml_major_version=`echo ${min_xml_version} | cut -d. -f1`
      min_xml_minor_version=`echo ${min_xml_version} | cut -d. -f2`
      min_xml_micro_version=`echo ${min_xml_version} | cut -d. -f3`

      xml_config_major_version=`${XML2_BIN_xml2_config} --version | cut -d. -f1`
      xml_config_minor_version=`${XML2_BIN_xml2_config} --version | cut -d. -f2`
      xml_config_micro_version=`${XML2_BIN_xml2_config} --version | cut -d. -f3`

      if ((xml_config_major_version > min_xml_major_version)) ||
         ((xml_config_major_version == min_xml_major_version && xml_config_minor_version > min_xml_minor_version)) ||
         ((xml_config_major_version == major && xml_config_minor_version == min_xml_minor_version && xml_config_micro_version >= min_xml_micro_version)); then
         AC_MSG_RESULT([yes ($xml_config_major_version.$xml_config_minor_version.$xml_config_micro_version)])
      else
         AC_MSG_RESULT([no ($xml_config_major_version.$xml_config_minor_version.$xml_config_micro_version)])
         XML2_INSTALLED="no"
      fi

      XML2_CFLAGS="${XML2_CFLAGS} -I${XML2_INCLUDES}/libxml2"
      XML2_CPPFLAGS="${XML2_CPPFLAGS} -I${XML2_INCLUDES}/libxml2"
      XML2_CXXFLAGS="${XML2_CXXFLAGS} -I${XML2_INCLUDES}/libxml2"
      AC_SUBST(XML2_CFLAGS)
      AC_SUBST(XML2_CPPFLAGS)
      AC_SUBST(XML2_CXXFLAGS)

      CFLAGS=${XML2_CFLAGS}
      AC_CHECK_HEADERS([libxml/parser.h libxml/xmlmemory.h], [], [$XML2_INSTALLED="no"])
   fi
   AX_FLAGS_RESTORE()

   AM_CONDITIONAL(HAVE_XML2, test "${XML2_INSTALLED}" = "yes")
   if test "$XML2_INSTALLED" = "yes" ; then
      AC_DEFINE([HAVE_XML2], [1], [Defined if libxml2 is available])
   fi
])

# AX_PROG_ELFUTILS
# --------------------
# Check for elfutils installation
AC_DEFUN([AX_PROG_ELFUTILS],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(elfutils,
      AC_HELP_STRING(
         [--with-elfutils@<:@=DIR@:>@],
         [Specify where to find elfutils' installation]
      ),
      [elfutils_paths="${withval}"],
      [elfutils_paths="no"]
   )

   if test "${elfutils_paths}" != "no" ; then
      # Search for elfutils installation
      AX_FIND_INSTALLATION([ELFUTILS], [${elfutils_paths}], [eu-addr2line], [], [], [], [], [], [], [])

      if test "${ELFUTILS_INSTALLED}" != "yes" ; then
         AC_MSG_ERROR([elfutils installation cannot be found. Please review --with-elfutils option.])
      else
         AC_DEFINE([HAVE_LIBADDR2LINE], [1], [Define to 1 if libaddr2line is available])
         elfutils_addr2line_configure_args="--with-elfutils-addr2line=${elfutils_paths}/bin/eu-addr2line"
      fi
   fi

   AX_FLAGS_RESTORE()
])

# AX_PROG_BINUTILS
# --------------------
# Check for binutils installation
AC_DEFUN([AX_PROG_BINUTILS],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(binutils,
      AC_HELP_STRING(
         [--with-binutils@<:@=DIR@:>@],
         [Specify where to find binutils' installation]
      ),
      [binutils_paths="${withval}"],
      [binutils_paths="no"]
   )

   if test "${binutils_paths}" != "no" ; then
      # Search for binutils installation
      AX_FIND_INSTALLATION([BINUTILS], [${binutils_paths}], [addr2line], [], [], [], [], [], [], [])

      if test "${BINUTILS_INSTALLED}" != "yes" ; then
         AC_MSG_ERROR([binutils installation cannot be found. Please review --with-binutils option.])
      else
         AC_DEFINE([HAVE_LIBADDR2LINE], [1], [Define to 1 if libaddr2line is available])
         binutils_addr2line_configure_args="--with-binutils-addr2line=${binutils_paths}/bin/addr2line"
      fi
   fi

   AX_FLAGS_RESTORE()
])

# AX_PROG_LIBADDR2LINE
# --------------------
AC_DEFUN([AX_PROG_LIBADDR2LINE],
[
   AC_REQUIRE([AX_PROG_ELFUTILS])
   AC_REQUIRE([AX_PROG_BINUTILS])

   LIBADDR2LINE_INSTALLED="no"
   LIBADDR2LINE_SRCDIR="\${top_srcdir}/libaddr2line/src"
   LIBADDR2LINE_CFLAGS="-I${LIBADDR2LINE_SRCDIR}"
   if test "${ELFUTILS_INSTALLED}" = "yes" -o "${BINUTILS_INSTALLED}" = "yes" ; then
      LIBADDR2LINE_INSTALLED="yes"
      LIBADDR2LINE_BLDDIR="\${top_builddir}/libaddr2line/src"
      LIBADDR2LINE_LIBADD="${LIBADDR2LINE_BLDDIR}/libaddr2line.la ${LIBADDR2LINE_BLDDIR}/libmaps.la"
      LIBADDR2LINE_LDADD="${LIBADDR2LINE_BLDDIR}/libaddr2line.la ${LIBADDR2LINE_BLDDIR}/libmaps.la"
      if test "${ELFUTILS_INSTALLED}" = "yes" ; then
         LIBADDR2LINE_LIBADD="${LIBADDR2LINE_LIBADD} ${LIBADDR2LINE_BLDDIR}/libsymtab.la"
         LIBADDR2LINE_LDADD="${LIBADDR2LINE_LDADD} ${LIBADDR2LINE_BLDDIR}/libsymtab.la"
      fi
      AC_SUBST(LIBADDR2LINE_LIBADD)
      AC_SUBST(LIBADDR2LINE_LDADD)
   fi
   AC_SUBST(LIBADDR2LINE_CFLAGS)
   AM_CONDITIONAL(HAVE_LIBADDR2LINE, test "${LIBADDR2LINE_INSTALLED}" = "yes")
])

# AX_CHECK_PERUSE
# ---------------------------
AC_DEFUN([AX_CHECK_PERUSE],
[
   AC_REQUIRE([AX_PROG_MPI])

   PERUSE_AVAILABILITY="no"
   AC_ARG_ENABLE(peruse,
      AC_HELP_STRING(
         [--enable-peruse],
         [Enable gathering information with PerUse]
      ),
      [enable_peruse="${enableval}"],
      [enable_peruse="auto"]
   )

   if test "${MPI_INSTALLED}" = "yes"; then
      if test "${enable_peruse}" = "auto" ; then
         AC_MSG_CHECKING(for peruse.h)
         if test -r ${MPI_INCLUDES}/peruse.h ; then
            AC_MSG_RESULT([available])
            enable_peruse="yes"
         else
            AC_MSG_RESULT([not available])
            enable_peruse="no"
         fi
      elif test "${enable_peruse}" = "yes" ; then
            AC_MSG_CHECKING(for peruse.h)
         if test -r ${MPI_INCLUDES}/peruse.h ; then
            AC_MSG_RESULT([available])
         else
            AC_MSG_NOTICE([Can not find the peruse header inside the MPI include directory.])
            AC_MSG_ERROR([Feature requested by the user but not available!])
         fi
      fi
   else
      enable_peruse="no"
   fi

   if test "${enable_peruse}" = "yes" ; then
      AC_MSG_CHECKING(for PERUSE_SUCCESS in peruse.h)
      AX_FLAGS_SAVE()
      CFLAGS="-I${MPI_INCLUDES}"
      AC_LANG_SAVE()
      AC_LANG([C])
      AC_TRY_COMPILE(
         [#include <peruse.h>],
         [
            int i = PERUSE_SUCCESS;
            return 0;
         ],
         [peruse_success="yes"],
         [peruse_success="no"]
      )
      AX_FLAGS_RESTORE()
      AC_LANG_RESTORE()

      if test "${peruse_success}" = "yes"; then
         AC_MSG_RESULT([available])
         AC_DEFINE([PERUSE_ENABLED], 1, [Determine if the PerUse API can be used])
         PERUSE_AVAILABILITY="yes"
      else
         AC_MSG_NOTICE([Can not find PERUSE_SUCCESS in peruse.h])
         AC_MSG_ERROR([Feature requested by the user but not available!])
      fi
   fi
])

# AX_PROG_MX
# ----------
AC_DEFUN([AX_PROG_MX],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(mx,
      AC_HELP_STRING(
         [--with-mx@<:@=DIR@:>@],
         [specify where to find MX libraries and includes]
      ),
      [mx_paths="$withval"],
      [mx_paths="/gpfs/apps/MX /opt/osshpc/mx"] dnl List of possible default paths
   )

   dnl Search for MX installation
   AX_FIND_INSTALLATION([MX], [$mx_paths], [], [], [], [], [], [], [], [])

   if test "$MX_INSTALLED" = "yes" ; then
      AC_CHECK_HEADERS([myriexpress.h], [], [MX_INSTALLED="no"])
      AC_CHECK_LIB([myriexpress], [mx_get_info], 
         [ 
           MX_LDFLAGS="${MX_LDFLAGS} -lmyriexpress"
           AC_SUBST(MX_LDFLAGS)
         ], 
         [ MX_INSTALLED="no" ]
      )
      AC_CHECK_HEADERS([mx_dispersion.h], [mx_dispersion_h_found="yes"], [mx_dispersion_h_found="no"])
      AC_CHECK_LIB([myriexpress], [mx_get_dispersion_counters], 
         [mx_get_dispersion_counters_found="yes"], 
         [mx_get_dispersion_counters="no"]
      )
      if test "$mx_dispersion_h_found" = "yes" -a "$mx_get_dispersion_counters_found" = "yes" ; then
         MX_CFLAGS="${MX_CFLAGS} -DMX_MARENOSTRUM_API"
         AC_SUBST(MX_CFLAGS)
         MX_CXXFLAGS="${MX_CFLAGS} -DMX_MARENOSTRUM_API"
         AC_SUBST(MX_CXXFLAGS)
      fi
   fi

   dnl Did the checks pass?
   AM_CONDITIONAL(HAVE_MX, test "${MX_INSTALLED}" = "yes")

   if test "$MX_INSTALLED" = "no" ; then
      AC_MSG_WARN([Myrinet MX counters tracing has been disabled])
   fi

   AX_FLAGS_RESTORE()
])

# AX_PROG_COUNTERS
# ----------------
AC_DEFUN([AX_PROG_COUNTERS],
[
   AC_REQUIRE([AX_PROG_PMAPI])
   AC_REQUIRE([AX_PROG_PAPI])

   if test "${papi_paths}" = "not_set" ; then
      if test "${target_os}" = "aix" ; then
         if test "${enable_pmapi}" = "not_set" ; then
            AC_MSG_ERROR([Attention! You're not indicating where to locate PAPI and if you want to use PMAPI. PAPI and PMAPI (specifically in AIX) allows gathering hardware performance counters. These counters are very useful to increase the richness of the final analysis. Please, use either --with-papi=DIR where DIR is the base location of the PAPI package or use --without-papi if you don't want to use PAPI in this installation. If you want to use PMAPI, please use --enable-pmapi, otherwise use --disable-pmapi.])
         fi
      else
         AC_MSG_ERROR([Attention! You're not indicating where to locate PAPI. PAPI allows gathering hardware performance counters. These counters are very useful to increase the richness of the final analysis. Please, use either --with-papi=DIR where DIR is the base location of the PAPI package or use --without-papi if you don't want to use PAPI in this installation.])
      fi
   fi

   if test "${PMAPI_ENABLED}" = "yes" -o "${PAPI_ENABLED}" = "yes" ; then
      AC_DEFINE([USE_HARDWARE_COUNTERS], 1, [Enable HWC support])
      use_hw_counters="1"
   else
      AC_DEFINE([USE_HARDWARE_COUNTERS], 0, [Disable HWC support])
      use_hw_counters="0"
   fi

   if test "${PMAPI_ENABLED}" = "yes" -a "${PAPI_ENABLED}" = "yes" ; then
      AC_MSG_ERROR([Error! Cannot use PMAPI and PAPI at the same time to access hardware counters!])
   fi
])


# AX_PROG_PMAPI
# -------------
AC_DEFUN([AX_PROG_PMAPI],
[
   AC_ARG_ENABLE(pmapi,
      AC_HELP_STRING(
         [--enable-pmapi],
         [Enable PMAPI library to gather CPU performance counters]
      ),
      [enable_pmapi="${enableval}"],
      [enable_pmapi="not_set"]
   )
   PMAPI_ENABLED="no"

   if test "${enable_pmapi}" = "yes" ; then
      PMAPI_ENABLED="yes"
      AC_CHECK_HEADERS([pmapi.h], [], [pmapi_h_notfound="yes"])

      if test "${pmapi_h_notfound}" = "yes" ; then
         AC_MSG_ERROR([Error! Unable to find pmapi.h])
      fi
   fi

   AM_CONDITIONAL(HAVE_PMAPI, test "${PMAPI_ENABLED}" = "yes")

   if test "${PMAPI_ENABLED}" = "yes" ; then
      AC_DEFINE([PMAPI_COUNTERS], [1], [PMAPI is used as API to gain access to CPU hwc])
   else
      if test "${enable_pmapi}" = "yes" ; then
         AC_MSG_ERROR([Error PMAPI was not found and was enabled at configure time!])
      fi
   fi
])

# AX_PROG_PAPI
# ------------
AC_DEFUN([AX_PROG_PAPI],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(papi,
      AC_HELP_STRING(
         [--with-papi@<:@=DIR@:>@],
         [specify where to find PAPI libraries and includes]
      ),
      [papi_paths="${withval}"],
      [papi_paths="not_set"] dnl List of possible default paths
   )

   if test "${IS_SPARC64_MACHINE}" = "yes" ; then
      if test -z "${papi_paths}" ; then
         AC_MSG_ERROR([Error PAPI was not found but was enabled at configure time!])
      fi
   fi

   AC_ARG_ENABLE(sampling,
      AC_HELP_STRING(
         [--enable-sampling],
         [Enable PAPI sampling support]
      ),
      [enable_sampling="${enableval}"],
      [enable_sampling="yes"]
   )

   dnl Search for PAPI installation, except for SPARC64 which is autoembedded
   if test "${IS_SPARC64_MACHINE}" != "yes" ; then
      AX_FIND_INSTALLATION([PAPI], [$papi_paths], [], [], [], [], [], [], [], [])
      PAPI_ENABLED="${PAPI_INSTALLED}"
   else
      papi_paths="/usr"
      PAPI_ENABLED="yes"
      PAPI_HOME=${papi_paths}
      AC_SUBST(PAPI_HOME)
   fi

   AM_CONDITIONAL(HAVE_PAPI_EMBEDDED, test "${IS_SPARC64_MACHINE}" = "yes")

   if test "${PAPI_ENABLED}" = "yes" ; then
      AC_CHECK_HEADERS([papi.h], [], [papi_h_notfound="yes"])

      if test "${papi_h_notfound}" = "yes" ; then
         AC_MSG_ERROR([Error! Unable to find papi.h])
      fi

      if test "${IS_BGL_MACHINE}" = "yes" ; then
         LIBS="-static -lpapi -L${BG_HOME}/bglsys/lib -lbgl_perfctr.rts -ldevices.rts -lrts.rts"
      elif test "${IS_BGP_MACHINE}" = "yes" ; then
         LIBS="-lpapi -L${BG_HOME}/runtime/SPI -lSPI.cna"
      elif test "${IS_BGQ_MACHINE}" = "yes" ; then
         LIBS="-lpapi -L${BG_HOME}/spi/lib -lSPI -lSPI_cnk -lstdc++ -lrt"
      elif test "${IS_SPARC64_MACHINE}" = "yes" ; then
         LIBS=""
      else
         if test "${OperatingSystem}" = "freebsd" ; then
            LIBS="-lpapi -lpmc"
         elif test "${OperatingSystem}" = "linux" -a "${Architecture}" = "powerpc" ; then
            LIBS="-lpapi"
            if test -d "${PAPI_HOME}/perfctr/lib" ; then
               LIBS="-L${PAPI_HOME}/perfctr/lib ${LIBS}"
            fi
         elif test "${OperatingSystem}" = "aix" -a "${Architecture}" = "powerpc" ; then
            if test "${BITS}" = "64" ; then
               if test -f "${PAPI_LIBSDIR}/libpapi64.a" -o -f "${PAPI_LIBSDIR}/libpapi64.so" ; then
                  LIBS="-lpapi64 -lpmapi"
               else
                  LIBS="-lpapi -lpmapi"
               fi 
            else
               LIBS="-lpapi -lpmapi"
            fi
         else
            LIBS="-lpapi"
         fi
      fi

      AC_CHECK_LIB([papi], [PAPI_start],
         [ 
            PAPI_LIBS="${LIBS} -lpfm"
            AC_SUBST(PAPI_LIBS)
         ],
         [PAPI_ENABLED="no"]
      )
   fi

   AM_CONDITIONAL(HAVE_PAPI, test "${PAPI_ENABLED}" = "yes")

   AC_DEFINE([SAMPLING_SUPPORT], [1], [Enable Sampling])

   if test "${PAPI_ENABLED}" = "yes" ; then
      PAPI_SAMPLING_ENABLED="no"
      AC_DEFINE([PAPI_COUNTERS], [1], [PAPI is used as API to gain access to CPU hwc])
      AC_DEFINE([NEW_HWC_SYSTEM], [1], [Enable HWC support])
      AC_MSG_NOTICE([PAPI and substrate libraries: ${PAPI_LIBS}])
      if test "${enable_sampling}" = "yes" ; then
         AC_CHECK_MEMBER([PAPI_substrate_info_t.supports_hw_overflow],[support_hw_overflow="yes"],[support_hw_overflow="no"],[#include <papi.h>])
         if test "${support_hw_overflow}" = "yes" ; then
            AC_DEFINE([HAVE_SUPPORT_HW_OVERFLOW], [1], [Use supports_hw_overflow field])
            AC_DEFINE([PAPI_SAMPLING_SUPPORT], [1], [Enable PAPI sampling support])
         else
            AC_CHECK_MEMBER([PAPI_substrate_info_t.hardware_intr_sig],[hardware_intr_sig="yes"],[hardware_intr_sig="no"],[#include <papi.h>])
            if test "${hardware_intr_sig}" = "yes" ; then
               AC_DEFINE([HAVE_HARDWARE_INTR_SIG], [1], [Use hardware_intr_sig field])
               AC_DEFINE([PAPI_SAMPLING_SUPPORT], [1], [Enable PAPI sampling support])
               PAPI_SAMPLING_ENABLED="yes"
            else
               AC_CHECK_MEMBER([PAPI_component_info_t.hardware_intr],[support_comp_hw="yes"],[support_comp_hw="no"],[#include <papi.h>])
               if test "${support_comp_hw}" = "yes" ; then
                  AC_DEFINE([HAVE_COMPONENT_HARDWARE_INTR], [1], [Use hardware_intr in PAPI_component_info_t field])
                  AC_DEFINE([PAPI_SAMPLING_SUPPORT], [1], [Enable PAPI sampling support])
                  PAPI_SAMPLING_ENABLED="yes"
               else
                  AC_MSG_NOTICE([Cannot determine how to check whether PAPI supports HW overflows! Will use software-based sampling.])
               fi
            fi
         fi
      fi
   else
      if test "${papi_paths}" != "no" -a "${papi_paths}" != "not_set"; then
         AC_MSG_ERROR([Error PAPI was not found but was enabled at configure time!])
      fi
   fi

   AX_FLAGS_RESTORE()
])

# AX_IS_ALTIX_MACHINE
# ----------------
AC_DEFUN([AX_IS_ALTIX_MACHINE],
[
   AC_MSG_CHECKING([if this is an Altix machine])
   if test -r /etc/sgi-release ; then 
      AC_MSG_RESULT([yes])
      IS_ALTIX_MACHINE="yes"
			AC_DEFINE([IS_ALTIX], 1, [Defined if this machine is a SGI Altix])
   else
      AC_MSG_RESULT([no])
      IS_ALTIX_MACHINE="no"
   fi
])

# AX_HAVE_MMTIMER_DEVICE
# ----------------
AC_DEFUN([AX_HAVE_MMTIMER_DEVICE],
[
   AC_REQUIRE([AX_IS_ALTIX_MACHINE])

   if test "${IS_ALTIX_MACHINE}" = "yes" ; then
      AC_MSG_CHECKING([if this is an Altix machine has MMTimer device])
      if test -r /dev/mmtimer ; then 
         AC_MSG_RESULT([yes])
         AC_DEFINE([HAVE_MMTIMER_DEVICE], 1, [Defined if this machine has a MMTimer device and it is readable])
         HAVE_MMTIMER_DEVICE="yes"
      else
         AC_MSG_RESULT([no])
         HAVE_MMTIMER_DEVICE="no"
      fi
   else
      HAVE_MMTIMER_DEVICE="no"
   fi
])

# AX_IS_CRAY_XT
# ---------------------
AC_DEFUN([AX_IS_CRAY_XT],
[
   AC_MSG_CHECKING([if this is a Cray XT machine])
   AC_ARG_ENABLE(check-cray-xt,
      AC_HELP_STRING(
         [--enable-check-cray-xt],
         [Enable check to known if this is a frontend to a Cray XT machine (enabled by default)]
      ),
      [enable_check_cxt="${enableval}"],
      [enable_check_cxt="yes"]
   )

   IS_CXT_MACHINE="no"
   if test "${enable_check_cxt}" = "yes" ; then
      if test -d /opt/cray ; then
         if test `which cc | grep xt-asyncpe | wc -l` != "0" ; then
           IS_CXT_MACHINE="yes"
         elif test `which cc | grep craype | wc -l` != "0" ; then
           IS_CXT_MACHINE="yes"
         fi
      fi
   fi
   AC_MSG_RESULT([$IS_CXT_MACHINE])
   AM_CONDITIONAL(IS_CRAY_XT_MACHINE, test "${IS_CXT_MACHINE}" = "yes")
])

# AX_IS_BGQ_MACHINE
# ---------------------
AC_DEFUN([AX_IS_BGQ_MACHINE],
[
   IS_BGQ_MACHINE="no"

   AC_MSG_CHECKING([if this is a BG/Q machine])
   AC_ARG_ENABLE(check-bgq,
      AC_HELP_STRING(
         [--enable-check-bgq],
         [Enable check to known if this is a frontend to a BG/Q BE machine (enabled by default)]
      ),
      [enable_check_bgq="${enableval}"],
      [enable_check_bgq="yes"]
   )
   if test "${enable_check_bgq}" = "yes" -a -d /bgsys/drivers/ppcfloor ; then
     if test -f /bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc64-bgq-linux-gcc  ; then
       IS_BGQ_MACHINE="yes"
       BG_HOME="/bgsys/drivers/ppcfloor"
       CFLAGS="${CFLAGS} -I/bgsys/drivers/ppcfloor/spi/include/kernel/cnk -I/bgsys/drivers/ppcfloor"
       AC_SUBST(BG_HOME)
       AC_DEFINE([IS_BGQ_MACHINE], 1, [Defined if this machine is a BG/Q machine])
     fi
   fi
   AC_MSG_RESULT($IS_BGQ_MACHINE)
   AM_CONDITIONAL(IS_BGQ_MACHINE, test "${IS_BGQ_MACHINE}" = "yes")
])

# AX_IS_BGP_MACHINE
# ---------------------
AC_DEFUN([AX_IS_BGP_MACHINE],
[
   IS_BGP_MACHINE="no"

   AC_MSG_CHECKING([if this is a BG/P machine])
   AC_ARG_ENABLE(check-bgp,
      AC_HELP_STRING(
         [--enable-check-bgp],
         [Enable check to known if this is a frontend to a BG/P BE machine (enabled by default)]
      ),
      [enable_check_bgp="${enableval}"],
      [enable_check_bgp="yes"]
   )

   if test "${enable_check_bgp}" = "yes" -a -d /bgsys/drivers/ppcfloor ; then
     if test -f /bgsys/drivers/ppcfloor/gnu-linux/bin/powerpc-bgp-linux-gcc ; then
       IS_BGP_MACHINE="yes"
       BG_HOME="/bgsys/drivers/ppcfloor"
       CFLAGS="${CFLAGS} -I${BG_HOME}/bglsys/include -I${BG_HOME}/arch/include -I${BG_HOME}/blrts-gnu/include"
       AC_SUBST(BG_HOME)
       AC_DEFINE([IS_BGP_MACHINE], 1, [Defined if this machine is a BG/P machine])
     fi
   fi
   AC_MSG_RESULT($IS_BGP_MACHINE)
   AM_CONDITIONAL(IS_BGP_MACHINE, test "${IS_BGP_MACHINE}" = "yes")
])

# AX_IS_BGL_MACHINE
# ---------------------
AC_DEFUN([AX_IS_BGL_MACHINE],
[
   AC_MSG_CHECKING([if this is a BG/L machine])
   AC_ARG_ENABLE(check-bgl,
      AC_HELP_STRING(
         [--enable-check-bgl],
         [Enable check to known if this is a frontend to a BG/L BE machine (enabled by default)]
      ),
      [enable_check_bgl="${enableval}"],
      [enable_check_bgl="yes"]
   )

   if test "${enable_check_bgl}" = "yes" -a -d /bgl/BlueLight/ppcfloor/bglsys ; then
     IS_BGL_MACHINE="yes"
     BG_HOME="/bgl/BlueLight/ppcfloor"
     CFLAGS="${CFLAGS} -I${BG_HOME}/bglsys/include -I${BG_HOME}/blrts-gnu/include"
     AC_SUBST(BG_HOME)
     AC_MSG_RESULT([yes])
     AC_DEFINE([IS_BGL_MACHINE], 1, [Defined if this machine is a BG/L machine])
   else
     IS_BGL_MACHINE="no"
     AC_MSG_RESULT([no])
   fi
   AM_CONDITIONAL(IS_BGL_MACHINE, test "${IS_BGL_MACHINE}" = "yes")
])

# AX_OPENMP
#-----------------
AC_DEFUN([AX_OPENMP],
[
   AC_PREREQ(2.59)

   AC_CACHE_CHECK([for OpenMP flag of _AC_LANG compiler],
      ax_cv_[]_AC_LANG_ABBREV[]_openmp,
      [save[]_AC_LANG_PREFIX[]FLAGS=$[]_AC_LANG_PREFIX[]FLAGS ax_cv_[]_AC_LANG_ABBREV[]_openmp=unknown
      # Flags to try:  -fopenmp (gcc), -openmp (icc), -mp (SGI &amp; PGI),
      #                -xopenmp (Sun), -omp (Tru64), -qsmp=omp (AIX), none
      ax_openmp_flags="-fopenmp -openmp -mp -xopenmp -omp -qsmp=omp none"
      if test "x$OPENMP_[]_AC_LANG_PREFIX[]FLAGS" != x; then
         ax_openmp_flags="$OPENMP_[]_AC_LANG_PREFIX[]FLAGS $ax_openmp_flags"
      fi
      for ax_openmp_flag in $ax_openmp_flags; do
         case $ax_openmp_flag in
            none) []_AC_LANG_PREFIX[]FLAGS=$save[]_AC_LANG_PREFIX[] ;;
            *) []_AC_LANG_PREFIX[]FLAGS="$save[]_AC_LANG_PREFIX[]FLAGS $ax_openmp_flag" ;;
         esac
         AC_TRY_LINK_FUNC(omp_set_num_threads,
   	       [ax_cv_[]_AC_LANG_ABBREV[]_openmp=$ax_openmp_flag; break])
      done
      []_AC_LANG_PREFIX[]FLAGS=$save[]_AC_LANG_PREFIX[]FLAGS])
      if test "x$ax_cv_[]_AC_LANG_ABBREV[]_openmp" = "xunknown"; then
         m4_default([$2],:)
      else
         if test "x$ax_cv_[]_AC_LANG_ABBREV[]_openmp" != "xnone"; then
            OPENMP_[]_AC_LANG_PREFIX[]FLAGS=$ax_cv_[]_AC_LANG_ABBREV[]_openmp
         fi
         m4_default([$1], [AC_DEFINE(HAVE_OPENMP,1,[Define if OpenMP is enabled])])
      fi
])

# AX_CHECK_UNWIND
# ------------
AC_DEFUN([AX_CHECK_UNWIND],
[
   AX_FLAGS_SAVE()

   libunwind_works="no"

   AC_ARG_WITH(unwind,
      AC_HELP_STRING(
         [--with-unwind@<:@=DIR@:>@],
         [specify where to find Unwind libraries and includes]
      ),
      [unwind_paths=${withval}],
      [unwind_paths="not_set"]
   )

   dnl Check for unwind, except on ppc!
   if test "${unwind_paths}" = "not_set" -a "${target_cpu}" != "powerpc" ; then
      AC_MSG_ERROR([You haven't specified the location of the libunwind through the --with-unwind parameter. The libunwind library allows Extrae gathering information of call-site locations at OpenMP and MPI calls, at sample points or whenever the user wants to collect them through the Extrae API. This data is very useful to attribute to a certain region of code any performance issue. You can download libunwind from: https://savannah.nongnu.org/projects/libunwind - download version 1.0.1 or higher. If you don't want to use libunwind, you can pass --without-unwind.])
   fi

   if test "${unwind_paths}" != "no" ; then

      AX_FIND_INSTALLATION([UNWIND], [$unwind_paths], [], [], [], [], [], [], [], [])

      if test "${UNWIND_INSTALLED}" = "yes" ; then 

         UNWIND_LIBS="-lunwind"
         AC_SUBST(UNWIND_LIBS)

         CFLAGS="${CFLAGS} ${UNWIND_CFLAGS}"
         LIBS="${LIBS} -lunwind"
         LDFLAGS="${LDFLAGS} ${UNWIND_LDFLAGS}"

         AC_MSG_CHECKING([if libunwind works])

         AC_TRY_LINK(
            [ #define UNW_LOCAL_ONLY
              #include <libunwind.h> ], 
            [ unw_cursor_t cursor;
              unw_context_t uc;
              unw_word_t ip;

              unw_getcontext(&uc);
              unw_init_local(&cursor, &uc);
              unw_step(&cursor);
              unw_get_reg(&cursor, UNW_REG_IP, &ip);
            ],
            [ libunwind_works="yes" ],
            [ libunwind_works="no" ]
         )

         AC_MSG_RESULT([${libunwind_works}])

      fi

      if test "${libunwind_works}" = "yes"; then
         AC_DEFINE([UNWIND_SUPPORT], [1], [Unwinding support enabled for IA64/x86-64])
         AC_DEFINE([HAVE_LIBUNWIND_H], [1], [Define to 1 if you have <libunwind.h> header file])
      else
         AC_MSG_ERROR([Cannot link libunwind test. Check that --with-unwind points to the appropriate libunwind directory.])
      fi
   fi
   AX_FLAGS_RESTORE()
])

# AX_CHECK_LIBZ
# ------------
AC_DEFUN([AX_CHECK_LIBZ],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(libz,
      AC_HELP_STRING(
         [--with-libz@<:@=DIR@:>@],
         [specify where to find libz libraries and includes]
      ),
      [libz_paths="${withval}"],
      [libz_paths="/usr/local /usr"] dnl List of possible default paths
   )

   AX_FIND_INSTALLATION([LIBZ], [${libz_paths}], [], [], [zlib.h], [], [z], [], [], [])

   if test "${LIBZ_INSTALLED}" = "yes"; then
      CFLAGS="${CFLAGS} ${LIBZ_CFLAGS}"
      LIBS="${LIBS} ${LIBZ_LIBS}"
      LDFLAGS="${LDFLAGS} ${LIBZ_LDFLAGS}"

      AC_CHECK_LIB(z, inflateEnd, [zlib_cv_libz=yes], [zlib_cv_libz=no])

      if test "${zlib_cv_libz}" = "yes"; then
         AC_DEFINE([HAVE_ZLIB], [1], [Zlib available])
         ZLIB_INSTALLED="yes"
      else
         ZLIB_INSTALLED="no"
      fi
   fi 

   AM_CONDITIONAL(HAVE_ZLIB, test "${ZLIB_INSTALLED}" = "yes")

   AX_FLAGS_RESTORE()
])

# AX_PROG_LIBEXECINFO
# -------------
AC_DEFUN([AX_PROG_LIBEXECINFO],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH(execinfo,
      AC_HELP_STRING(
         [--with-execinfo=@<:@=DIR@:>@],
         [specify where to find execinfo libraries and includes (FreeBSD/Darwin specific?)]
      ),
      [execinfo_paths="${withval}"],
      [execinfo_paths="no"]
   )

   if test "${execinfo_paths}" != "no" ; then
      AX_FIND_INSTALLATION([execinfo], [${execinfo_paths}], [], [], [], [], [], [], [], [])
      if test "${EXECINFO_INSTALLED}" = "yes" ; then
        if test -f "${EXECINFO_HOME}/lib/execinfo.a" -o \
                -f "${EXECINFO_HOME}/lib/execinfo.so" ; then
           if test -f ${EXECINFO_HOME}/include/execinfo.h ; then
              CFLAGS="-I ${XECINFO_HOME}/include"
              LIBS="-L ${EXECINFO_HOME}/lib -lexecinfo"
		          AC_TRY_LINK(
		              [ #include <execinfo.h> ],
		              [ backtrace ((void*)0, 0); ],
		              [ execinfo_links="yes" ]
		            )
              if test "${execinfo_links}" = "yes" ; then
                 AC_DEFINE([HAVE_EXECINFO_H], 1, [Define to 1 if you have the <execinfo.h> header file.])
              else
                 AC_MSG_ERROR([Cannot link using execinfo... See config.log for further details])
              fi
           else
              AC_MSG_ERROR([Cannot find execinfo header files in ${execinfo_paths}/include, maybe you can install it from /usr/ports/devel/libexecinfo])
           fi
        else
           AC_MSG_ERROR([Cannot find execinfo library files in ${execinfo_paths}/lib, maybe you can install it from /usr/ports/devel/libexecinfo])
        fi
      fi
   fi

   AX_FLAGS_RESTORE()
])

# AX_PROG_LIBDWARF
# -------------
AC_DEFUN([AX_PROG_LIBDWARF],
[
   libdwarf_found="no"
   AX_FLAGS_SAVE()

   AC_ARG_WITH(dwarf,
      AC_HELP_STRING(
         [--with-dwarf=@<:@=DIR@:>@],
         [specify where to find libdwarf libraries and includes]
      ),
      [dwarf_paths="${withval}"],
      [dwarf_paths="no"]
   )

   if test -z "${dwarf_paths}" ; then
      AC_MSG_ERROR([Cannot find DWARF library])
   fi

   if test "${dwarf_paths}" != "no" ; then
      AX_FIND_INSTALLATION([DWARF], [${dwarf_paths}], [], [], [], [], [], [], [], [])
      if test "${DWARF_INSTALLED}" = "yes" ; then
        if test -f "${DWARF_LIBSDIR}/libdwarf.a" -o \
                -f "${DWARF_LIBSDIR}/libdwarf.so" ; then
           if test -f "${DWARF_HOME}/include/libdwarf.h" -a \
                   -f "${DWARF_HOME}/include/dwarf.h" ; then
              libdwarf_found="yes"
           elif test -f "${DWARF_HOME}/include/libdwarf/libdwarf.h" -a \
                     -f "${DWARF_HOME}/include/libdwarf/dwarf.h" ; then
              libdwarf_found="yes"
           else
              AC_MSG_ERROR([Cannot find DWARF header files in ${dwarf_paths}/include])
           fi
        elif test -f "${DWARF_LIBSDIR_MULTIARCH}/libdwarf.a" -o \
                  -f "${DWARF_LIBSDIR_MULTIARCH}/libdwarf.so" ; then
           if test -f "${DWARF_HOME}/include/libdwarf.h" -a \
                   -f "${DWARF_HOME}/include/dwarf.h" ; then
              libdwarf_found="yes"
              DWARF_LIBSDIR="${DWARF_LIBSDIR_MULTIARCH}"
           elif test -f "${DWARF_HOME}/include/libdwarf/libdwarf.h" -a \
                     -f "${DWARF_HOME}/include/libdwarf/dwarf.h" ; then
              libdwarf_found="yes"
              DWARF_LIBSDIR="${DWARF_LIBSDIR_MULTIARCH}"
           else
              AC_MSG_ERROR([Cannot find DWARF header files in ${dwarf_paths}/include])
           fi
        else
           AC_MSG_ERROR([Cannot find DWARF library files in ${dwarf_paths}/lib])
        fi
      fi
   fi

   AX_FLAGS_RESTORE()
])


# AX_PROG_LIBELF
# -------------
AC_DEFUN([AX_PROG_LIBELF],
[
   libelf_found="no"
   AX_FLAGS_SAVE()

   AC_ARG_WITH(elf,
      AC_HELP_STRING(
         [--with-elf=@<:@=DIR@:>@],
         [specify where to find libelf libraries and includes]
      ),
      [elf_paths="${withval}"],
      [elf_paths="no"]
   )

   if test -z "${elf_paths}" ; then
      AC_MSG_ERROR([Cannot find ELF library])
   fi

   if test "${elf_paths}" != "no" ; then
      AX_FIND_INSTALLATION([ELF], [${elf_paths}], [], [], [], [], [], [], [], [])
      if test "${ELF_INSTALLED}" = "yes" ; then
        if test -f "${ELF_LIBSDIR}/libelf.a" -o \
                -f "${ELF_LIBSDIR}/libelf.so" ; then
           if test -f "${ELF_INCLUDES}/libelf/libelf.h" -o \
                   -f "${ELF_INCLUDES}/libelf.h"  ; then
              libelf_found="yes"
           else
              AC_MSG_ERROR([Cannot find ELF header files neither in ${ELF_INCLUDES} nor in ${ELF_INCLUDES}/libelf])
           fi
        elif test -f "${ELF_LIBSDIR_MULTIARCH}/libelf.a" -o \
                  -f "${ELF_LIBSDIR_MULTIARCH}/libelf.so" ; then
           if test -f "${ELF_INCLUDES}/libelf/libelf.h" -o \
                   -f "${ELF_INCLUDES}/libelf.h"  ; then
              libelf_found="yes"
              ELF_LIBSDIR="${ELF_LIBSDIR_MULTIARCH}"
           else
              AC_MSG_ERROR([Cannot find ELF header files neither in ${ELF_INCLUDES} nor in ${ELF_INCLUDES}/libelf])
           fi
        else
           AC_MSG_ERROR([Cannot find ELF library files in ${ELF_LIBSDIR}])
        fi
      fi
   fi

   AX_FLAGS_RESTORE()
])


# AX_PROG_SYNAPSE
# ----------------
AC_DEFUN([AX_PROG_SYNAPSE],
[
  AX_FLAGS_SAVE()
  AC_LANG_SAVE()
  AC_LANG([C++])

  AC_ARG_WITH(synapse,
    AC_HELP_STRING(
      [--with-synapse@<:@=DIR@:>@],
      [specify where to find Synapse libraries and includes]
    ),
    [synapse_paths="$withval"],
    [synapse_paths="not_set"] # List of possible default paths
  )

  dnl Search for Synapse installation
  AX_FIND_INSTALLATION([SYNAPSE], [$synapse_paths], [], [], [], [], [], [], [], [])

  if test "$SYNAPSE_INSTALLED" = "yes" ; then
    dnl Check for headers

    AC_MSG_CHECKING([for MRNetApp.h presence])
    if test -e ${SYNAPSE_INCLUDES}/MRNetApp.h; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
      SYNAPSE_INSTALLED="no"
    fi
    
    dnl Check for libraries
    AC_MSG_CHECKING([for libsynapse_frontend])

    if test -f "${SYNAPSE_LIBSDIR}/libsynapse_frontend.a" -o \
       -f "${SYNAPSE_LIBSDIR}/libsynapse_frontend.so" ; then
      SYNAPSE_FE_LIBS="-lsynapse_frontend"
      AC_SUBST(SYNAPSE_FE_LIBS)
      AC_MSG_RESULT([yes])
    else
      SYNAPSE_INSTALLED="no"
      AC_MSG_RESULT([no])
    fi

    AC_MSG_CHECKING([for libsynapse_backend])
    
    if test -f "${SYNAPSE_LIBSDIR}/libsynapse_backend.a" -o \
        -f "${SYNAPSE_LIBSDIR}/libsynapse_backend.so"; then
      SYNAPSE_BE_LIBS="-lsynapse_backend"
      AC_SUBST(SYNAPSE_BE_LIBS)
      AC_MSG_RESULT([yes])
    else
      SYNAPSE_INSTALLED="no"
      AC_MSG_RESULT([no])
    fi
  fi

  if test "${SYNAPSE_INSTALLED}" = "yes" ; then
    SYNAPSE_CONFIG="${SYNAPSE_HOME}/bin/synapse-config"
    AC_SUBST(SYNAPSE_CONFIG)
  fi

  AX_FLAGS_RESTORE()
  AC_LANG_RESTORE()

  AM_CONDITIONAL(HAVE_SYNAPSE, test "x$SYNAPSE_INSTALLED" = "xyes")
])

AC_DEFUN([AX_PROG_CLUSTERING],
[
  AX_FLAGS_SAVE()
  AC_ARG_WITH(clustering,
    AC_HELP_STRING(
      [--with-clustering@<:@=DIR@:>@],
      [specify where to find clustering libraries and includes]
    ),
    [clustering_paths="$withval"],
    [clustering_paths="not_set"] dnl List of possible default paths
  )
  dnl Search for Clustering installation
  AX_FIND_INSTALLATION([CLUSTERING], [$clustering_paths], [], [], [], [], [], [], [], [])

  dnl Check for TreeDBSCAN online libraries
  if test "${CLUSTERING_INSTALLED}" = "yes" ; then
    AC_MSG_CHECKING([for libTDBSCAN-fe])
    if test -f "${CLUSTERING_LIBSDIR}/libTDBSCAN-fe.so" ; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
      CLUSTERING_INSTALLED="no"
    fi

    AC_MSG_CHECKING([for libTDBSCAN-be-online])
    if test -f "${CLUSTERING_LIBSDIR}/libTDBSCAN-be-online.so" ; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
      CLUSTERING_INSTALLED="no"
    fi
  fi

  if test "${CLUSTERING_INSTALLED}" = "yes" ; then
    CLUSTERING_LIBS="-lTDBSCAN-fe -lTDBSCAN-be-online"

    AC_SUBST(CLUSTERING_LIBS)
    AC_DEFINE([HAVE_CLUSTERING], 1, [Define to 1 if CLUSTERING is installed in the system])
  fi

  AM_CONDITIONAL(HAVE_CLUSTERING, test "x${CLUSTERING_INSTALLED}" = "xyes")

  AX_FLAGS_RESTORE()
])

AC_DEFUN([AX_PROG_SPECTRAL],
[
  AX_FLAGS_SAVE()

  AC_ARG_WITH(spectral,
    AC_HELP_STRING(
      [--with-spectral@<:@=DIR@:>@],
      [specify where to find spectral analysis libraries and includes]
    ),
    [spectral_paths="$withval"],
    [spectral_paths="not_set"] dnl List of possible default paths
  )

  dnl Search for Spectral Analysis installation
  AX_FIND_INSTALLATION([SPECTRAL], [$spectral_paths], [], [], [], [], [], [], [], [])
  
  spectral_works="no"
  if test "x${SPECTRAL_INSTALLED}" = "xyes" ; then
    LIBS="-L${SPECTRAL_HOME} -lspectral"

    AC_MSG_CHECKING([whether libspectral has unresolved dependencies with libfft])
    AC_TRY_LINK(
      [ #include <stdio.h>
        #include <spectral-api.h> ],
      [ Spectral_AllocateSignal(0); ],
      [ spectral_works="yes" ]
    )
    if test "${spectral_works}" = "yes" ; then
      AC_MSG_RESULT([no])
    else
      dnl There are unresolved dependencies with fftw3
      AC_MSG_RESULT([yes])
      AC_ARG_WITH(fft,
        AC_HELP_STRING(
          [--with-fft@<:@=DIR@:>@],
          [specify where to find FFT libraries and includes]
        ),
        [fft_paths="$withval"],
        [fft_paths="not_set"] dnl List of possible default paths
      )
      dnl Search for FFT installation
      AX_FIND_INSTALLATION([FFT], [$fft_paths], [], [], [], [], [], [], [], [])

      LIBS="${LIBS} ${FFT_LDFLAGS} -lfftw3 -lm"
      AC_TRY_LINK(
        [ #include <stdio.h>
          #include <spectral-api.h> ],
        [ Spectral_AllocateSignal(0); ],
        [ spectral_works="yes" ]
      )
    fi
    AC_MSG_CHECKING([whether a program can be linked with libspectral])
    if test "${spectral_works}" = "yes" ; then
      SPECTRAL_LIBS="${LIBS}"
      AC_SUBST(SPECTRAL_LIBS)
      AC_DEFINE([HAVE_SPECTRAL], 1, [Define to 1 if SPECTRAL ANALYSIS is installed in the system])
    fi
    AC_MSG_RESULT([$spectral_works])
  fi
  SPECTRAL_INSTALLED=$spectral_works
  AM_CONDITIONAL(HAVE_SPECTRAL, test "x${spectral_works}" = "xyes")
  AX_FLAGS_RESTORE()
])

AC_DEFUN([AX_PROG_ONLINE],
[
  AC_REQUIRE([AX_PROG_MPI])
  AC_REQUIRE([AX_PROG_SYNAPSE])
  AC_REQUIRE([AX_PROG_CLUSTERING])
  AC_REQUIRE([AX_PROG_SPECTRAL])
  AC_REQUIRE([AX_PROG_XML2])

  AC_ARG_ENABLE(online,
    AC_HELP_STRING(
      [--enable-online],
      [Enable on-line analysis]
    ),
    [ONLINE_enabled="${enableval}"],
    [ONLINE_enabled="no"]
  )

  AC_ARG_ENABLE(inotify,
    AC_HELP_STRING(
      [--enable-inotify],
      [Enable inotify support]
    ),
    [INOTIFY_enabled="${enableval}"],
    [INOTIFY_enabled="no"]
  )

  if test "x$INOTIFY_enabled" = "xyes" ; then
    AC_DEFINE([HAVE_INOTIFY], 1,
              [Define this if inotify is supported])
  fi

  if test "$ONLINE_enabled" = "yes" ; then
   if test "${XML2_INSTALLED}" != "yes" ; then
      AC_MSG_WARN([You enabled the on-line analysis mode, but a required dependency is missing!])
      AC_MSG_ERROR([Please enable support for XML with --with-xml])
   fi
  fi

  if test "$ONLINE_enabled" = "yes" ; then
    AX_ENSURE_CXX_PRESENT([on-line analysis])
  fi

  # Check if the dependencies are installed
  have_online="no"
  if test "x$ONLINE_enabled" = "xyes" ; then
    AC_MSG_CHECKING([for the on-line analysis dependencies])

    analyzers=0
    if test "x$CLUSTERING_INSTALLED" = "xyes" ; then
      let "analyzers++"
    fi
    if test "x$SPECTRAL_INSTALLED"   = "xyes" ; then
      let "analyzers++"
    fi

    if   test "x$MPI_INSTALLED"        != "xyes" ; then
      AC_MSG_RESULT([MPI is missing!])
      AC_MSG_WARN([You enabled the on-line analysis mode, but a required dependency is missing!])
      AC_MSG_ERROR([Please specify a working installation of MPI with --with-mpi])
    elif test "x$SYNAPSE_INSTALLED"   != "xyes" ; then
      AC_MSG_RESULT([Synapse libraries are missing!])
      AC_MSG_WARN([You enabled the on-line analysis mode, but a required dependency is missing!])
      AC_MSG_ERROR([Please specify a working installation of Synapse libraries with --with-synapse])
    elif [[ $analyzers -eq 0 ]] ; then
      AC_MSG_RESULT([analysis tools are missing!])
      AC_MSG_WARN([You enabled the on-line analysis mode, but a required dependency is missing!])
      AC_MSG_ERROR([Please specify a working installation of the analysis tools with --with-clustering and/or --with-spectral])
    else
      have_online="yes"
      AC_MSG_RESULT([$have_online])
    fi
  fi
  AM_CONDITIONAL(HAVE_ONLINE, test "x${have_online}" = "xyes")
])

AC_DEFUN([AX_CHECK_WEAK_ALIAS_ATTRIBUTE],
[
  # Test whether compiler accepts __attribute__ form of weak aliasing
  AC_CACHE_CHECK([whether ${CC} accepts function __attribute__((weak,alias()))],
  [ax_cv_weak_alias_attribute], [

    # We add -Werror if it's gcc to force an error exit if the weak attribute
    # isn't understood

    save_CFLAGS=${CFLAGS}
    
    if test "${GCC}" = "yes" ; then
       CFLAGS="-Werror"
    elif test "`basename ${CC}`" = "xlc" ; then
       CFLAGS="-qhalt=i"
    fi

    if test "${pgi_compiler}" = "no" ; then
       # Try linking with a weak alias...
       AC_LINK_IFELSE([
         AC_LANG_PROGRAM([
            void __weakf(int c) {}
            void weakf(int c) __attribute__((weak, alias("__weakf")));],
            [weakf(0)])],
         [ax_cv_weak_alias_attribute="yes"],
         [ax_cv_weak_alias_attribute="no"])
     else
        ax_cv_weak_alias_attribute="no"
     fi

     # Restore original CFLAGS
     CFLAGS=${save_CFLAGS}
  ])

  # What was the result of the test?
  AS_IF([test "${ax_cv_weak_alias_attribute}" = "yes"],
  [
    AC_DEFINE([HAVE_WEAK_ALIAS_ATTRIBUTE], 1,
              [Define this if weak aliases may be created with __attribute__])
  ])
])

AC_DEFUN([AX_CHECK_ALIAS_ATTRIBUTE],
[
  # Test whether compiler accepts __attribute__ form of aliasing
  AC_CACHE_CHECK([whether ${CC} accepts function __attribute__((alias()))],
  [ax_cv_alias_attribute], [

    # We add -Werror if it's gcc to force an error exit if the weak attribute
    # isn't understood

    save_CFLAGS=${CFLAGS}
    
    if test "${GCC}" = "yes" ; then
       CFLAGS="-Werror"
    elif test "`basename ${CC}`" = "xlc" ; then
       CFLAGS="-qhalt=i"
    fi

    if test "${pgi_compiler}" = "no" ; then
       # Try linking with a weak alias...
       AC_LINK_IFELSE([
         AC_LANG_PROGRAM([
            void __alias(int c) {}
            void alias(int c) __attribute__((alias("__alias")));],
         [alias(0)])],
         [ax_cv_alias_attribute="yes"],
         [ax_cv_alias_attribute="no"])
     else
        ax_cv_alias_attribute="no"
     fi

     # Restore original CFLAGS
     CFLAGS=${save_CFLAGS}
     ])

  # What was the result of the test?
  AS_IF([test "${ax_cv_alias_attribute}" = "yes"],
  [
    AC_DEFINE([HAVE_ALIAS_ATTRIBUTE], 1,
              [Define this if aliases may be created with __attribute__])
  ])
])

AC_DEFUN([AX_CHECK_UNUSED_ATTRIBUTE],
[
  # Test whether compiler accepts __attribute__ form of setting unused 
  AC_CACHE_CHECK([whether ${CC} accepts function __attribute__((unused))],
  [ax_cv_unused_attribute], [

    # We add -Werror if it's gcc to force an error exit if the weak attribute
    # isn't understood

    save_CFLAGS=${CFLAGS}
    
    if test "${GCC}" = "yes" ; then
       CFLAGS="-Werror"
    elif test "`basename ${CC}`" = "xlc" ; then
       CFLAGS="-qhalt=i"
    fi

    if test "${pgi_compiler}" = "no" ; then
       # Try linking with a weak alias...
       AC_LINK_IFELSE([
         AC_LANG_PROGRAM(
         [
            static char var __attribute__((unused));],
            [])],
         [ax_cv_unused_attribute="yes"],
         [ax_cv_unused_attribute="no"])
     else
        ax_cv_unused_attribute="no"
     fi

    # Restore original CFLAGS
    CFLAGS=${save_CFLAGS}
  ])

  # What was the result of the test?
  AS_IF([test "${ax_cv_unused_attribute}" = "yes"],
  [
    AC_DEFINE([HAVE_UNUSED_ATTRIBUTE], 1,
              [Define this if variables/functions can be marked as unused])
  ])
])

AC_DEFUN([AX_OFF_T_64BIT],
[
	AC_MSG_CHECKING([how to get 64-bit off_t])
	if test "${OperatingSystem}" = "linux" ; then
		AC_DEFINE([_FILE_OFFSET_BITS],[64],[Define the bits for the off_t structure])
		AC_MSG_RESULT([define _FILE_OFFSET_BITS=64])
	elif test "${OperatingSystem}" = "freebsd" ; then
		AC_MSG_RESULT([nothing required])
	else
		AC_MSG_RESULT([unknown])
	fi
])

AC_DEFUN([AX_CHECK_PROC_CPUINFO],
[
	AC_MSG_CHECKING(for /proc/cpuinfo)
	if test -r /proc/cpuinfo ; then
		AC_MSG_RESULT([found])
		AC_DEFINE([HAVE_PROC_CPUINFO], 1, [Define to 1 if the OS has /proc/cpuinfo])
	else
		AC_MSG_RESULT([not found])
	fi
])

AC_DEFUN([AX_CHECK_PROC_MEMINFO],
[
	AC_MSG_CHECKING(for /proc/meminfo)
	if test -r /proc/meminfo ; then
		AC_MSG_RESULT([found])
		AC_DEFINE([HAVE_PROC_MEMINFO], 1, [Define to 1 if the OS has /proc/meminfo])
	else
		AC_MSG_RESULT([not found])
	fi
])

AC_DEFUN([AX_CHECK_GETCPU],
[
	AC_CHECK_HEADERS([sched.h])
	AC_CHECK_FUNC(sched_getcpu, [AC_DEFINE([HAVE_SCHED_GETCPU],[1],[Define to 1 if have sched_getcpu])])
])

AC_DEFUN([AX_PROG_MEMKIND],
[
  AC_ARG_WITH(memkind,
    AC_HELP_STRING(
      [--with-memkind@<:@=DIR@:>@],
      [specify where to find memkind libraries and includes]
    ),
    [memkind_paths="$withval"],
    [memkind_paths="not_set"] dnl List of possible default paths
  )

  dnl Search for Spectral Analysis installation
  AX_FIND_INSTALLATION([MEMKIND], [$memkind_paths], [], [], [], [], [], [], [], [])

  if test "x${MEMKIND_INSTALLED}" = "xyes" ; then
    AX_FLAGS_SAVE()
    CFLAGS="${MEMKIND_CFLAGS}"
    AC_CHECK_HEADERS([memkind.h], [MEMKIND_H_FOUND="yes"], [MEMKIND_H_FOUND="no"])
    AX_FLAGS_RESTORE()

    MEMKIND_LIBS="-lmemkind"
    AC_SUBST(MEMKIND_LIBS)
    AC_DEFINE([HAVE_MEMKIND], 1, [Define to 1 if MEMKIND is available])
  fi

  AM_CONDITIONAL(HAVE_MEMKIND, test "x${MEMKIND_H_FOUND}" = "xyes" )
])
