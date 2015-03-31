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
// require("js/omv/module/admin/service/subsonic/Backup.js")

Ext.define("OMV.module.admin.service.subsonic.Settings", {
    extend : "OMV.workspace.form.Panel",
    requires: [
        "OMV.data.Model",
        "OMV.data.Store",
        "OMV.module.admin.service.subsonic.Backup"
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
                { name  : "enable", value : true },
                { name  : "showtab", value : true }
            ],
            properties : "show"
        }]
    }],

    getButtonItems: function() {
        var items = this.callParent(arguments);

        items.push({
            id: this.getId() + "-show",
            xtype: "button",
            name: "showbutton",
            text: _("Open Web Client"),
            icon: "images/subsonic.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
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
            id: this.getId() + "-update",
            xtype: "button",
            name: "updatesub",
            text: _("Normal Update"),
            icon: "images/subsonic.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
            handler : function() {
                var me = this;
                OMV.MessageBox.show({
                    title   : _("Confirmation"),
                    msg     : _("Update/Downgrade SubSonic?"),
                    buttons : Ext.Msg.YESNO,
                    fn      : function(answer) {
                        if (answer !== "yes")
                           return;
                       OMV.Rpc.request({
                           scope   : me,
                           rpcData : {
                                service : "Subsonic",
                                method  : "doUpdateSuB",
                                params  : {
                                    update   : 0
                                }
                            },
                            success : function(id, success, response) {
                                me.doReload();
                                OMV.MessageBox.hide();
                            }
                        });
                    },
                    scope : me,
                    icon  : Ext.Msg.QUESTION
                });
            }
        },{
            id: this.getId() + "-updateb",
            xtype: "button",
            name: "updatesubb",
            text: _("Beta Update"),
            icon: "images/subsonic.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
            handler : function() {
                var me = this;
                OMV.MessageBox.show({
                    title   : _("Confirmation"),
                    msg     : _("Install Beta version of SubSonic?"),
                    buttons : Ext.Msg.YESNO,
                    fn      : function(answer) {
                        if (answer !== "yes")
                           return;
                       OMV.Rpc.request({
                           scope   : me,
                           rpcData : {
                                service : "Subsonic",
                                method  : "doUpdateSuBB",
                                params  : {
                                    update   : 0
                                }
                            },
                            success : function(id, success, response) {
                                me.doReload();
                                OMV.MessageBox.hide();
                            }
                        });
                    },
                    scope : me,
                    icon  : Ext.Msg.QUESTION
                });
            }
        }, {
            id: this.getId() + "-backup",
            xtype: "button",
            text: _("Backup/restore"),
            icon: "images/wrench.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            scope: this,
            handler: function() {
                Ext.create("OMV.module.admin.service.subsonic.Backup").show();
            }
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
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "settings",
    path      : "/service/subsonic",
    text      : _("Settings"),
    position  : 10,
    className : "OMV.module.admin.service.subsonic.Settings"
});

