cmake_minimum_required(VERSION 2.8)
PROJECT(izumo C)

SET(IZUMO_ACCESS_FILE "${CMAKE_INSTALL_PREFIX}/share/izumo/hosts.izumo")
SET(IZUMO_DICHOME     "${CMAKE_INSTALL_PREFIX}/share/izumo/dic")
SET(IZUMO_ERRDIR      "${CMAKE_INSTALL_PREFIX}/share/izumo/log")
SET(IR_UNIX_DIR       "/tmp/.iroha_unix")
SET(IR_UNIX_SOCKNAME  "IROHA")

INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include/RK)

CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/include/config.h.in ${PROJECT_SOURCE_DIR}/include/config.h)

# build izumooyashiro
ADD_EXECUTABLE(izumooyashiro 
  server/comm.c 
  server/convert.c 
  server/main.c 
  server/misc.c 
  server/session.c 
  server/util.c 
  server/wconvert.c)
ADD_LIBRARY(rkindep STATIC 
  lib/RKindep/basename.c 
  lib/RKindep/cfuncs.c 
  lib/RKindep/cksum.c 
  lib/RKindep/file.c 
  lib/RKindep/strops.c)
ADD_LIBRARY(rk STATIC 
  lib/RK/bits.c 
  lib/RK/bun.c 
  lib/RK/context.c 
  lib/RK/dd.c 
  lib/RK/dic.c 
  lib/RK/dicsw.c 
  lib/RK/fq.c 
  lib/RK/kana.c 
  lib/RK/ncache.c 
  lib/RK/ngram.c 
  lib/RK/nword.c 
  lib/RK/permdic.c 
  lib/RK/tempdic.c 
  lib/RK/util.c)

TARGET_LINK_LIBRARIES(izumooyashiro rkindep)
TARGET_LINK_LIBRARIES(izumooyashiro rk)

# build dic
ADD_EXECUTABLE(crfreq  dic/cmd/crfreq.c)
TARGET_LINK_LIBRARIES(crfreq rk)

ADD_EXECUTABLE(crxdic  dic/cmd/crxdic.c)
TARGET_LINK_LIBRARIES(crxdic rkindep)
TARGET_LINK_LIBRARIES(crxdic rk)

ADD_EXECUTABLE(crxgram dic/cmd/crxgram.c)
TARGET_LINK_LIBRARIES(crxgram rkindep)

ADD_EXECUTABLE(forcpp  dic/cmd/forcpp.c)
ADD_EXECUTABLE(forsort dic/cmd/forsort.c)
ADD_EXECUTABLE(kpdic   dic/cmd/kpdic.c)
ADD_EXECUTABLE(mergewd dic/cmd/mergewd.c)
ADD_EXECUTABLE(pod     dic/cmd/pod.c)

ADD_CUSTOM_TARGET(iroha
  COMMAND cat ${PROJECT_SOURCE_DIR}/dic/ideo/pubdic/pubdic.p | ${PROJECT_BINARY_DIR}/pod - -p -i -2 > ${PROJECT_BINARY_DIR}/iroha.spl
  COMMAND ${PROJECT_BINARY_DIR}/mergewd < ${PROJECT_BINARY_DIR}/iroha.spl > ${PROJECT_BINARY_DIR}/iroha.t
  COMMAND rm ${PROJECT_BINARY_DIR}/iroha.spl
  COMMAND ${PROJECT_BINARY_DIR}/crxdic -D ${PROJECT_BINARY_DIR}/cnj.bits -m -o iroha.cbd -n iroha ${PROJECT_BINARY_DIR}/iroha.t
  COMMAND ${PROJECT_BINARY_DIR}/crfreq -div 512 -o iroha.cld iroha.cbd iroha.mwd
  COMMAND cat ${PROJECT_BINARY_DIR}/bushu.cbd >> ${PROJECT_BINARY_DIR}/iroha.cbd
)

ADD_CUSTOM_TARGET(fuzokugo
  COMMAND cp ${PROJECT_SOURCE_DIR}/dic/ideo/grammar/main.code ${PROJECT_BINARY_DIR}
  COMMAND cp ${PROJECT_SOURCE_DIR}/dic/ideo/grammar/gram.goto ${PROJECT_BINARY_DIR}
  COMMAND ${PROJECT_BINARY_DIR}/forcpp -7 < ${PROJECT_SOURCE_DIR}/dic/ideo/grammar/gram.code | cpp -USX | ${PROJECT_BINARY_DIR}/forcpp -8 > ${PROJECT_BINARY_DIR}/cpp.gram
  COMMAND ${PROJECT_BINARY_DIR}/crxgram -f ${PROJECT_BINARY_DIR}/cpp.gram
  COMMAND ${PROJECT_BINARY_DIR}/forsort -7 < ${PROJECT_BINARY_DIR}/cnj.swd | sort | ${PROJECT_BINARY_DIR}/forsort  -8 | ${PROJECT_BINARY_DIR}/mergewd > ${PROJECT_BINARY_DIR}/fuzokugo.swd
  COMMAND ${PROJECT_BINARY_DIR}/crxdic -g -s -o fuzokugo.cbd -D cnj.bits -n fuzokugo ${PROJECT_BINARY_DIR}/fuzokugo.swd
  COMMAND ${PROJECT_BINARY_DIR}/crxdic -D cnj.bits -m -o bushu.cbd -n bushu ${PROJECT_SOURCE_DIR}/dic/ideo/words/bushu.t
)

ADD_DEPENDENCIES(iroha pod)
ADD_DEPENDENCIES(iroha mergewd)
ADD_DEPENDENCIES(iroha fuzokugo)
ADD_DEPENDENCIES(iroha crxdic)
ADD_DEPENDENCIES(iroha crfreq)
ADD_DEPENDENCIES(fuzokugo forcpp)
ADD_DEPENDENCIES(fuzokugo crxgram)
ADD_DEPENDENCIES(fuzokugo forsort)
ADD_DEPENDENCIES(fuzokugo mergewd)
ADD_DEPENDENCIES(fuzokugo crxdic)
ADD_DEPENDENCIES(izumooyashiro iroha)
ADD_DEPENDENCIES(izumooyashiro fuzokugo)

INSTALL(TARGETS izumooyashiro DESTINATION bin)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/server/hosts.izumo DESTINATION share/izumo)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/dic/phono/default.kpdef DESTINATION share/izumo/dic RENAME default.ctd)
INSTALL(FILES ${PROJECT_BINARY_DIR}/iroha.cbd DESTINATION share/izumo/dic/canna)
INSTALL(FILES ${PROJECT_BINARY_DIR}/fuzokugo.cbd DESTINATION share/izumo/dic/canna)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/dic/ideo/words/dics.dir DESTINATION share/izumo/dic/canna)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/dic/ideo/words/hojomwd.t DESTINATION share/izumo/dic/canna RENAME hojomwd.ctd)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/dic/ideo/words/hojoswd.t DESTINATION share/izumo/dic/canna RENAME hojoswd.ctd)
INSTALL(FILES ${PROJECT_BINARY_DIR}/bushu.cbd DESTINATION share/izumo/dic/canna RENAME bushu.cld)
INSTALL(DIRECTORY DESTINATION share/izumo/dic/user)
INSTALL(DIRECTORY DESTINATION share/izumo/dic/group)
INSTALL(DIRECTORY DESTINATION share/izumo/log)