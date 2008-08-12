<?php
define( MEMCACHED_HOST, '127.0.0.1' );
define( MEMCACHED_PORT, '11211' );
define( MEMCACHED_TIMEOUT, '10' );

if( !extension_loaded( 'memcache' ) )
{
    die( 'Extension not loaded' );
}
if( $cr = memcache_connect( MEMCACHED_HOST, MEMCACHED_PORT, MEMCACHED_TIMEOUT ) )
{
        $key = 'mod_memcached_include_tests';
        $fileContents = 'SSI block added at ' . date( 'Y/m/d H:i:s', time(  ) );
        $flag = false;

        for( $i = 1; $i < 3; $i++)
        {
            $finalKey = $key . '_' . $i;
            $fileFinalContents = $fileContents . '::' . $i;

            print( 'Adding file : ' . $finalKey . ' with contents '. $fileFinalContents . chr( 10 ) );

            //the file does not exists
            if( !memcache_replace( $cr, $finalKey, $fileFinalContents, $flag, 0 ) )
            {
                if( !memcache_add( $cr, $finalKey, $fileFinalContents, $flag, 0 ) )
                {
                    print( 'Unable to add file' );
                }
            }
        }
}
else
{
    print( 'unable to connect to memcached' );
}
?>
