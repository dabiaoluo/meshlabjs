/* 
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

//Console output redirecting ...

//(function(){
//    var oldLog = console.log;
//    console.log = function (message) {
//        MLJ.gui.Log.append(message);
//        oldLog.apply(console, arguments);
//    };
//})();

MLJ.gui = {};

MLJ.gui.Widget = function (build, refresh) {
    this._build = jQuery.isFunction(build) ? build : function () {
    };
    this._refresh = jQuery.isFunction(refresh) ? refresh : function () {
    };
};

MLJ.gui.SearchElement = function (tag, handler) {
    this.tag = tag;
    this.handler = jQuery.isFunction(handler) ? handler : function () {
    };
};

MLJ.gui.Layer = function (name, visible) {
    this.name = name;
    this.visible = visible;
};

MLJ.gui.Tab = function (name, content) {
    this.name = name;
    this.content = content;
    this.jQueryTab = function () {
        return $('<li><a href="#tab-' + name + '"><span>' + name + '</span></a></li>');
    };
    this.jQueryTabContent = function () {
        return $('<div id="tab-' + name + '">' + content + '</div>');
    };
};

MLJ.gui.Button = function (id, title, text, imageSrc) {
    this.id = id;
    this.title = title;
    this.text = text;
    this.imageSrc = imageSrc;
    this.$button = $('<button id="' + this.id + '" title="' + this.title + '"></button>');
    this.$button.append('<img src="' + this.imageSrc + '"/>').button().tooltip();
};

MLJ.gui.Button.prototype = {
    jQueryButton: function () {
        return this.$button;
    }
};

MLJ.gui.FileButton = function (id, title, text, imageSrc) {
    MLJ.gui.Button.call(this, id, title, text, imageSrc);
    var _$file = $('<input type="file" />');

    this.jQueryFile = function () {
        return _$file;
    };

    this.multiple = function () {
        _$file.attr("multiple", "multiple");
    };

    //Enable click event
    this.$button.click(function () {
        _$file.click();
    });

};

//Pseudo inheritance
MLJ.extend(MLJ.gui.Button, MLJ.gui.FileButton);


MLJ.gui.PiP = function (x, y, id) {
    this.id = id;
    var ids = !id ? "" : " id='" + id + "'";

    var _$PiP;

    function init() {
        _$PiP = $("<div" + ids + "></div>").css({
            position: "absolute",
            top: y,
            left: x
        });
    }

    this.jQuery = function () {
        return _$PiP;
    };

    this.appendContent = function (content) {
        _$PiP.append(content);
    };

    this.setX = function (value) {
        _$PiP.css("left", value);
    };

    this.setY = function (value) {
        _$PiP.css("top", value);
    };

    this.setVisible = function (visible) {
        if (visible) {
            _$PiP.show();
        } else {
            _$PiP.hide();
        }
        return _$PiP;
    };


    var kw, kh;
    this.lock = function (xCoord, yCoord) {

        if (!xCoord && !yCoord) {
            xCoord = yCoord = true;
        }

        //Distance from window right edge
        kw = $(window).width() - (_$PiP.position().left + _$PiP.width());
        kh = $(window).height() - (_$PiP.position().top + _$PiP.height());

        $(window).resize(function () {
            var x = $(window).width() - kw - _$PiP.width();
            var y = $(window).height() - kh - _$PiP.height();

            if (xCoord) {
                _$PiP.offset({left: x});
            }
            if (yCoord) {
                _$PiP.offset({top: y});
            }
        });
    };

    init();
};

(function () {

    var _$pane = $('<div id="tools-pane"></div>');
    var _$3D = $('<div id="_3D"></div>');
    var _widgets = [];

    function update() {
        for (var i = 0, m = _widgets.length; i < m; i++) {
            _$pane.append(_widgets[i]._refresh());
        }

        _$3D.css({
            width: $(window).width() - _$pane.outerWidth(),
            left: _$pane.outerWidth()
        });
    }

    this.addWidget = function (widget) {
        if (widget instanceof MLJ.gui.Widget) {
            _widgets.push(widget);
        } else {
            console.error("The parameter must be an instance of MLJ.gui.Widget");
        }
    };

    this.makeGUI = function (title) {

        _$pane.append('<div id="top" ><span>' + title + '</span></div>');
        $('body').append(_$3D).append(_$pane);

        for (var i = 0, m = _widgets.length; i < m; i++) {
            _$pane.append(_widgets[i]._build());
        }

        _$3D.css({
            position: "absolute",
            width: $(window).width() - _$pane.outerWidth(),
            left: _$pane.outerWidth(),
            height: "100%",
            top: 0
        });

    };

    this.update = function () {
        update();
    };

    $(window).resize(function () {
        update();
    });

//    _$pane.click(function () {        
//        if (_$pane.is(":visible")) {
//            _$pane.hide("slide", {direction: "left"}, 500);           
//        } else {
//            _$pane.show("slide", {direction: "left"}, 500);          
//        }
//    });



}).call(MLJ.gui);