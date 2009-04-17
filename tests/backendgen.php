<?php
define( 'MEMCACHED_HOST', '127.0.0.1' );
define( 'MEMCACHED_PORT', '11211' );
define( 'MEMCACHED_TIMEOUT', '10' );

if( !extension_loaded( 'memcache' ) )
{
    die( 'Extension not loaded' );
}
if( $cr = memcache_connect( MEMCACHED_HOST, MEMCACHED_PORT, MEMCACHED_TIMEOUT ) )
{
    $fileContents = 'SSI block added at ' . date( 'Y/m/d H:i:s', time(  ) );
    $flag = false;

    $finalKey = md5( time(  ) + rand( 0, 100 ) ) . '.html';
    $fileFinalContents = file_get_contents( './lipsum.txt');

    //the file does not exists
    if( !memcache_replace( $cr, $finalKey, $fileFinalContents, $flag, 0 ) )
    {
        if( !memcache_add( $cr, $finalKey, $fileFinalContents, $flag, 0 ) )
        {
            print( 'Unable to add file' );
        }
    }

    echo '<!--#include memcached="' . $finalKey . '" -->';
}
else
{
    print( 'unable to connect to memcached' );
}
?>