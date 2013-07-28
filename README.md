pfc
===

Install

1. PHP extension:
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
2. webpfc
     if you web document root is /var/www, just copy the webpfc into /var/www/webpfc
     and now you can use the web gui by goto http://your server name/webpfc

if any question about pfc, please email to: lightning_heat@163.com
