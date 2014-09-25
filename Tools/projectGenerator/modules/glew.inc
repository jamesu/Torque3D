<?php

beginModule( 'glew' );	

// Source
addLibSrcDir( 'glew/src' );

// Additional includes
addLibIncludePath( 'glew/include' );

addProjectDefines( 'GLEW_STATIC;' );

endModule();
?>