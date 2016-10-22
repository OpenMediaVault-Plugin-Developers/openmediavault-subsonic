/**
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/form/Panel.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/form/plugin/LinkedFields.js")

Ext.define("OMV.module.admin.service.subsonic.Settings", {
    extend : "OMV.workspace.form.Panel",
    requires: [
        "OMV.data.Model",
        "OMV.data.Store"
    ],

    rpcService   : "Subsonic",
    rpcGetMethod : "getSettings",
    rpcSetMethod : "setSettings",

    initComponent: function() {
        this.on("load", function() {
            var checked = this.findField("enable").checked;
            var showtab = this.findField("showtab").checked;
            var parent = this.up("tabpanel");

            if (!parent) {
                return;
            }

            var managementPanel = parent.down("panel[title=" + _("Web Interface") + "]");

            if (managementPanel) {
                checked ? managementPanel.enable() : managementPanel.disable();
                showtab ? managementPanel.tab.show() : managementPanel.tab.hide();
            }
        }, this);

        this.callParent(arguments);
    },

    plugins      : [{
        ptype        : "linkedfields",
        correlations : [{
            name       : [
                "updatesub",
            ],
            conditions : [
                { name  : "update", value : false }
            ],
            properties : "!show"
        },{
            name       : [
                "updatesubb",
            ],
            conditions : [
                { name  : "bupdate", value : false }
            ],
            properties : "!show"
        },{
            name       : [
                "update",
            ],
            properties : "!show"
        },{
            name       : [
                "bupdate",
            ],
            properties : "!show"
        },{
            name       : [
                "showbutton",
            ],
            conditions : [
                { name  : "enable", value : true }
            ],
            properties : "show"
        }]
    }],

    getButtonItems: function() {
        var me = this;
        var items = this.callParent(arguments);
        items.push({
            id: me.getId() + "-show",
            xtype: "button",
            name: "showbutton",
            text: _("Open Web Client"),
            icon: "images/subsonic.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: me,
            handler: function() {
                var proxy = this.getForm().findField("ppass").getValue();
                if (proxy == true) {
                    var link = "http://" + location.hostname + "/subsonic/";
                } else {
                    var port = this.getForm().findField("port").getValue();
                    var link = "http://" + location.hostname + ":" + port;
                }
                window.open(link, "_blank");
            }
        },{
            id: me.getId() + "-update",
            xtype: "button",
            name: "updatesub",
            text: _("Install latest"),
            icon: "images/add.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: me,
            handler  : Ext.Function.bind(me.onCommandButton, me, [ "doUpdateSuB" ]) 
        },{
            id: me.getId() + "-updateb",
            xtype: "button",
            name: "updatesubb",
            text: _("Install beta"),
            icon: "images/add.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: me,
            handler  : Ext.Function.bind(me.onCommandButton, me, [ "doUpdateSuBB" ]) 
        }, {
            id: this.getId() + "-backup",
            xtype: "button",
            text: _("Backup"),
            icon: "images/wrench.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
            handler: Ext.Function.bind(this.onBackupButton, this)
        }, {
            id: this.getId() + "-restore",
            xtype: "button",
            text: _("Restore"),
            icon: "images/wrench.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
            handler: Ext.Function.bind(this.onRestoreButton, this),
        });

        return items;
    },

    getFormItems : function() {
        var me = this;

        return [{
            xtype    : "fieldset",
            title    : "General settings",
            defaults : {
                labelSeparator : ""
            },
            items : [{
                xtype         : "combo",
                name          : "fork",
                fieldLabel    : _("Change Fork"),
                queryMode     : "local",
                store : [
                    [ "SubSonic", _("SubSonic") ],
                    [ "MusicCabinet", _("MusicCabinet") ],
                    [ "Libresonic", _("Libresonic") ],
                    [ "FutureSonic", _("FutureSonic") ],
                    [ "Madsonic", _("Madsonic") ]
                ],
                editable      : false,
                triggerAction : "all",
                value         : "."
            },{
                
                xtype      : "checkbox",
                name       : "enable",
                boxLabel   : _("Subsonic can take a few seconds to start."),
                fieldLabel : _("Enable"),
                checked    : false
            },{
                xtype      : "checkbox",
                name       : "showtab",
                fieldLabel : _("Show Tab"),
                boxLabel   : _("Show tab containing Subsonic web interface frame."),
                checked    : false
            },{
                xtype: "numberfield",
                name: "port",
                fieldLabel: _("Port"),
                vtype: "port",
                minValue: 1024,
                maxValue: 65535,
                allowDecimals: false,
                allowBlank: false,
                value: 4040
            },{
                xtype      : "checkbox",
                name       : "ssl",
                fieldLabel : _("HTTPS"),
                boxLabel   : _("Auto enable HTTPS."),
                checked    : false
            },{
                xtype      : "checkbox",
                name       : "ppass",
                fieldLabel : _("Proxy Pass"),
                boxLabel   : _("Enable this to access via OMV_IP/subsonic"),
                checked    : false
            },{
                border: false,
                html: "<br />"
            },{
                xtype       : "textfield",
                name        : "msg",
                fieldLabel  : _("Version info"),
                submitValue : false,
                readOnly    : true
            },{
                border: false,
                html: "<br />"
            },{
                xtype   : "checkbox",
                name    : "update"
            },{
                xtype   : "checkbox",
                name    : "bupdate"
            }]
        }];
    },

    onBackupButton: function() {
        OMV.Download.request("Subsonic", "downloadBackup");
    },

    onRestoreButton: function() {
        Ext.create("OMV.window.Upload", {
            title: _("Upload backup"),
            service: "Subsonic",
            method: "uploadBackup",
            listeners: {
                scope: this,
                success: function(wnd, response) {
                    OMV.MessageBox.info(_("Restored backup"), _("Backup was successfully restored."));
                }
            }
        }).show();
    },

    onCommandButton : function(cmd) {
        var me = this;
        if(cmd == "doUpdateSuB") {
            title = _("Install latest version ...");
        } else {
            title = _("Install beta version ...");
        }
        Ext.create("OMV.window.Execute", {
            title          : title,
            rpcService     : "Subsonic",
            rpcMethod      : "doCommand",
            rpcParams      : {
                "command" : cmd
            },
            hideStopButton : true,
            listeners      : {
                scope     : me,
                finish    : function(wnd, response) {
                    wnd.appendValue(_("Done..."));
                    wnd.setButtonDisabled("close", false);
                },
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                },
                close     : function() {
                    me.doReload(); 
                    OMV.MessageBox.hide();
                }
            }
        }).show();
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "settings",
    path      : "/service/subsonic",
    text      : _("Settings"),
    position  : 10,
    className : "OMV.module.admin.service.subsonic.Settings"
});


