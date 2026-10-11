INCLUDE_DIRECTORIES(
     .
     gui
     ${CMAKE_BINARY_DIR}
     ${CMAKE_BINARY_DIR}/plugins/Calenduh/src
     ${CMAKE_BINARY_DIR}/plugins/Calenduh/src/gui
)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

SET(CALENDUH_SRCS
     Calenduh.hpp
     Calenduh.cpp
)
IF (STELLARIUM_GUI_MODE STREQUAL "Standard")
LIST(APPEND CALENDUH_SRCS
     gui/CalenduhDialog.hpp
     gui/CalenduhDialog.cpp
)

SET(CALENDUH_UIS
     gui/calenduhDialog.ui
)
ENDIF (STELLARIUM_GUI_MODE STREQUAL "Standard")

################# compiles resources files ############
SET(CALENDUH_RES ../resources/Calenduh.qrc)
IF (${QT_VERSION_MAJOR} EQUAL "5")
     IF (STELLARIUM_GUI_MODE STREQUAL "Standard")
          QT5_WRAP_UI(CALENDUH_UIS_H ${CALENDUH_UIS})
     ENDIF()
     QT5_ADD_RESOURCES(CALENDUH_RES_CXX ${CALENDUH_RES})
ELSE()
     IF (STELLARIUM_GUI_MODE STREQUAL "Standard")
          QT_WRAP_UI(CALENDUH_UIS_H ${CALENDUH_UIS})
     ENDIF()
     QT_ADD_RESOURCES(CALENDUH_RES_CXX ${CALENDUH_RES})
ENDIF()

ADD_LIBRARY(Calenduh-static STATIC ${CALENDUH_SRCS} ${CALENDUH_RES_CXX} ${CALENDUH_UIS_H})
TARGET_LINK_LIBRARIES(Calenduh-static Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGL)
SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES OUTPUT_NAME "Calenduh")
SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
ADD_DEPENDENCIES(AllStaticPlugins Calenduh-static)

SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES FOLDER "plugins/Calenduh")
