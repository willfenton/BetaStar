# Debug Bot source files
file(GLOB SOURCES_SC2LADDERSERVER_BETASTAR "*.cpp" "*.h")

# Include directories
include_directories(SYSTEM
    ${PROJECT_SOURCE_DIR}/tests/BetaStar
    ${PROJECT_SOURCE_DIR}/s2client-api/include
    ${PROJECT_BINARY_DIR}/s2client-api/generated
)

# Create the executable.
add_executable(BetaStar ${SOURCES_SC2LADDERSERVER_BETASTAR})
target_link_libraries(BetaStar
    sc2api sc2lib sc2utils
)