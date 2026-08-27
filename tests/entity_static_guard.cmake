set(entity_sources
  "${COMPONENT_DIR}/climate.cpp"
  "${COMPONENT_DIR}/climate.h"
  "${COMPONENT_DIR}/number.h"
  "${COMPONENT_DIR}/switch.h"
  "${COMPONENT_DIR}/button.h"
  "${COMPONENT_DIR}/entity_control.h"
  "${COMPONENT_DIR}/climate_session.h"
)

foreach(entity_source IN LISTS entity_sources)
  file(READ "${entity_source}" contents)
  string(TOLOWER "${contents}" contents_lower)
  string(REGEX MATCH "#include[ \\t]+[<\"](protocol[^>\"]*|messages\\.h|controllers\\.h)[>\"]" protocol_include "${contents_lower}")
  if(protocol_include)
    message(FATAL_ERROR "Entity source includes protocol implementation detail: ${entity_source}: ${protocol_include}")
  endif()
  string(REGEX MATCH "0xaa|0x55|aacommand|checksum|raw[_a-z0-9]*|packet[_a-z0-9]*|commandframe|wire[_a-z0-9]*" raw_protocol_knowledge "${contents_lower}")
  if(raw_protocol_knowledge)
    message(FATAL_ERROR "Entity source contains raw protocol knowledge: ${entity_source}: ${raw_protocol_knowledge}")
  endif()
endforeach()
