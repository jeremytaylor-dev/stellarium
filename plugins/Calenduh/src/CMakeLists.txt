INCLUDE_DIRECTORIES(
     .
     gui
     chart
     ${CMAKE_BINARY_DIR}
     ${CMAKE_BINARY_DIR}/plugins/Calenduh/src
     ${CMAKE_BINARY_DIR}/plugins/Calenduh/src/gui
)

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/src)

# Chart model, renderer and exporter: Qt only, no Stellarium dependency
# (architecture section 4), so that they can be tested stand-alone.
ADD_LIBRARY(CalenduhChart STATIC
     chart/ChartModel.hpp
     chart/ChartModel.cpp
     chart/ChartStyle.hpp
     chart/ChartRenderer.hpp
     chart/ChartRenderer.cpp
     chart/ChartExporter.hpp
     chart/ChartExporter.cpp
)
TARGET_LINK_LIBRARIES(CalenduhChart Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Svg)
SET_TARGET_PROPERTIES(CalenduhChart PROPERTIES FOLDER "plugins/Calenduh")

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
TARGET_LINK_LIBRARIES(Calenduh-static CalenduhChart Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::OpenGL)
SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES OUTPUT_NAME "Calenduh")
SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
ADD_DEPENDENCIES(AllStaticPlugins Calenduh-static)

SET_TARGET_PROPERTIES(Calenduh-static PROPERTIES FOLDER "plugins/Calenduh")

IF(ENABLE_TESTING)
     # Stand-alone renderer: chart-model JSON -> PNG/SVG/PDF
     ADD_EXECUTABLE(calenduh-render test/calenduh-render.cpp)
     TARGET_LINK_LIBRARIES(calenduh-render CalenduhChart Qt${QT_VERSION_MAJOR}::Gui)
     SET_TARGET_PROPERTIES(calenduh-render PROPERTIES FOLDER "plugins/Calenduh/test")
ENDIF(ENABLE_TESTING)
