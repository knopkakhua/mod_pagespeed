(function(){var c=window,d=document,g="prototype",h=function(a,b,e){if(a.addEventListener)a.addEventListener(b,e,!1);else if(a.attachEvent)a.attachEvent("on"+b,e);else{var f=a["on"+b];a["on"+b]=function(){e.call(this);f&&f.call(this)}}};c.pagespeed=c.pagespeed||{};var k=c.pagespeed,l=function(){this.a=this.c=!1};l[g].b=function(a){for(var b=0;b<a.length;++b){var e=a[b].getAttribute("pagespeed_high_res_src");e&&a[b].setAttribute("src",e)}};l[g].replaceElementSrc=l[g].b;
l[g].f=function(){if(this.c)this.a=!1;else{var a=d.body,b,e=0,f=this;"ontouchstart"in a?(h(a,"touchstart",function(){b=Date.now()}),h(a,"touchend",function(a){e=Date.now();(null!=a.changedTouches&&2==a.changedTouches.length||null!=a.touches&&2==a.touches.length||500>e-b)&&m(f)})):h(c,"click",function(){m(f)});h(c,"load",function(){m(f)});this.c=!0}};l[g].registerLazyLoadHighRes=l[g].f;var m=function(a){a.a||(a.d(),a.a=!0)};l[g].d=function(){this.b(d.getElementsByTagName("img"));this.b(d.getElementsByTagName("input"))};
l[g].replaceWithHighRes=l[g].d;k.e=function(){k.delayImages=new l};k.delayImagesInit=k.e;})();