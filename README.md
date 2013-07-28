pfc
===

Install

PHP extension:
  pfc extension
     #cd pfc
     #phpize
     #./configure --with-php-config=php-config
     #make 
     #make install
  pfc_memcache extension
     #cd pfc/storage/memcached
     #phpize
     #./confiugre --with-php-config=php-config
     #make
     #make install
  configuration
     edit php.ini
        this is an example
           extension="memcached.so"
           extension="pfc.so"
           extension="pfc_memcached.so"
           pfc.data_dir="/opt/samba/pfcstorage"
           pfc.profiler_enable="off"
           pfc.config_file="cache_functions"
           pfc.storage_module="memcached"
           pfc.memcached.servers="127.0.0.1:11211"
   
webpfc
     if you web document root is /var/www, just copy the webpfc into /var/www/webpfc
     and now you can use the web gui by goto http://your server name/webpfc
     install firefox addon https://addons.mozilla.org/en-US/firefox/addon/php-function-cache/
if any question about pfc, please email to: lightning_heat@163.com
