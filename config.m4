PHP_ARG_WITH(yaz,for YAZ support,
[  --with-yaz             Include YAZ support (ANSI/NISO Z39.50)])

if test "$PHP_YAZ" = "yes"; then
  AC_MSG_CHECKING([for YAZ libraries by calling pkg-config --exists yaz])
  if pkg-config --exists yaz; then
    AC_MSG_RESULT([found])
    if pkg-config --atleast-version 3.0.2 yaz; then
      AC_DEFINE(HAVE_YAZ,1,[Whether you have YAZ])
      lib=`pkg-config --libs yaz`
      for c in $lib; do
       case $c in
        -L*)
         dir=`echo $c|cut -c 3-|sed 's%/\.libs%%g'`
         PHP_ADD_LIBPATH($dir,YAZ_SHARED_LIBADD)
        ;;
       -l*)
         lib=`echo $c|cut -c 3-`
         PHP_ADD_LIBRARY($lib,,YAZ_SHARED_LIBADD)
        ;;
      esac
      done
      inc=`pkg-config --cflags yaz`
      PHP_EVAL_INCLINE($inc)
      PHP_NEW_EXTENSION(yaz, php_yaz.c, $ext_shared)
      PHP_SUBST(YAZ_SHARED_LIBADD)
    else
      AC_MSG_ERROR([YAZ is too old. 3.0.2 or later required])
    fi
  else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([YAZ development libraries missing])
  fi
elif test "$PHP_YAZ" != "no"; then
  AC_MSG_ERROR([with-yaz argument unsupported. Do not specify])
fi
