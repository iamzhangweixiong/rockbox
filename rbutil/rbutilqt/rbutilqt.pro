SOURCES += rbutilqt.cpp \
           main.cpp \
 install.cpp \
 httpget.cpp \
 configure.cpp \
 zip/zip.cpp \
 zip/unzip.cpp \
 installzip.cpp \
 installbootloader.cpp \
 installbl.cpp \
 progressloggergui.cpp \
 installtalkwindow.cpp \
 talkfile.cpp \
 autodetection.cpp \
 ../ipodpatcher/ipodpatcher.c \
 ../sansapatcher/sansapatcher.c \
 irivertools/irivertools.cpp \
 irivertools/md5sum.cpp  \
 browsedirtree.cpp

HEADERS += rbutilqt.h \
 settings.h \
 install.h \
 httpget.h \
 configure.h \
 zip/zip.h \
 zip/unzip.h \
 zip/zipentry_p.h \
 zip/unzip_p.h \
 zip/zip_p.h \
 version.h \
 installzip.h \
 installbootloader.h \
 installbl.h \
 installtalkwindow.h \
 talkfile.h \
 autodetection.h \
 progressloggerinterface.h \
 progressloggergui.h \
 ../ipodpatcher/ipodpatcher.h \
 ../ipodpatcher/ipodio.h \
 ../ipodpatcher/parttypes.h \
 ../sansapatcher/sansapatcher.h \
 ../sansapatcher/sansaio.h \
 irivertools/irivertools.h \
 irivertools/md5sum.h \
 irivertools/h100sums.h \ 
 irivertools/h120sums.h \
 irivertools/h300sums.h \
 irivertools/checksums.h \
 browsedirtree.h 

TEMPLATE = app
CONFIG += release \
          warn_on \
          thread \
          qt
TARGET = rbutilqt

FORMS += rbutilqtfrm.ui \
 aboutbox.ui \
 installfrm.ui \
 installprogressfrm.ui \
 configurefrm.ui \
 installbootloaderfrm.ui \
 browsedirtreefrm.ui \
 installtalkfrm.ui 

RESOURCES += rbutilqt.qrc

TRANSLATIONS += rbutil_de.ts
QT += network
DEFINES += RBUTIL _LARGEFILE64_SOURCE

win32 {
    SOURCES +=  ../ipodpatcher/ipodio-win32.c
    SOURCES +=  ../sansapatcher/sansaio-win32.c
}

unix {
    SOURCES +=  ../ipodpatcher/ipodio-posix.c
    SOURCES +=  ../sansapatcher/sansaio-posix.c
}


