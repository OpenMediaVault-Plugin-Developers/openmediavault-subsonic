/**
 * Copyright (C) 2013 OpenMediaVault Plugin Developers
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
    uses   : [
        "OMV.data.Model",
        "OMV.data.Store",
        "OMV.module.admin.service.subsonic.UpdateSUB",
        "OMV.module.admin.service.subsonic.UpdateSUBB"
    ],

    initComponent : function () {
        var me = this;

        me.on('load', function () {
            var checked = me.findField('enable').checked;
            var showtab = me.findField('showtab').checked;
            var parent = me.up('tabpanel');

            if (!parent)
                return;

            var managementPanel = parent.down('panel[title=' + _("Web Interface") + ']');

            if (managementPanel) {
                checked ? managementPanel.enable() : managementPanel.disable();
                showtab ? managementPanel.tab.show() : managementPanel.tab.hide();
            }
        });

        me.callParent(arguments);
    },

    rpcService   : "Subsonic",
    rpcGetMethod : "getSettings",
    rpcSetMethod : "setSettings",

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
        }]
    }],

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
                xtype   : "button",
                name    : "opensubsonic",
                text    : _("Subsonic Web Interface"),
                scope   : this,
                handler : function() {
                    var me = this;
                    var port = me.getForm().findField("port").getValue();
                    var link = "http://" + location.hostname + ":" + port + "/";
                    window.open(link, "_blank");
                },
                margin : "0 0 5 0"
            
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
                xtype   : "button",
                name    : "updatesub",
                text    : _("Update Subsonic"),
                scope   : this,
                handler : function() {
                    var me = this;
                    OMV.MessageBox.show({
                        title   : _("Confirmation"),
                        msg     : _("Are you sure you want to update Subsonic?"),
                        buttons : Ext.Msg.YESNO,
                        fn      : function(answer) {
                            if (answer !== "yes")
                               return;

                            OMV.Rpc.request({
                                scope   : me,
                                rpcData : {
                                    service : "Subsonic",
                                    method  : "doUpdateSUB",
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
                border: false,
                html: "<br />"
            },{
                xtype   : "checkbox",
                name    : "bupdate"
            },{
                xtype   : "button",
                name    : "updatesubb",
                text    : _("Update Subsonic Beta"),
                scope   : this,
                handler : function() {
                    var me = this;
                    OMV.MessageBox.show({
                        title   : _("Confirmation"),
                        msg     : _("Are you sure you want to update Subsonic Beta?"),
                        buttons : Ext.Msg.YESNO,
                        fn      : function(answer) {
                            if (answer !== "yes")
                               return;
                            // throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, "You CAN NOT use this branch with this repository.");

                            OMV.Rpc.request({
                                scope   : me,
                                rpcData : {
                                    service : "Subsonic",
                                    method  : "doUpdateSUBB",
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
                border: false,
                html: "<br />"
            }]
                },{
                        xtype: "fieldset",
                        title: _("Backup User Settings"),
                        fieldDefaults: {
                                labelSeparator: ""
                        },
                        items : [{
                xtype         : "combo",
                name          : "mntentref",
                fieldLabel    : _("Volume"),
                emptyText     : _("Select a volume ..."),
                allowBlank    : false,
                allowNone     : false,
                editable      : false,
                triggerAction : "all",
                displayField  : "description",
                valueField    : "uuid",
                store         : Ext.create("OMV.data.Store", {
                    autoLoad : true,
                    model    : OMV.data.Model.createImplicit({
                        idProperty : "uuid",
                        fields     : [
                            { name : "uuid", type : "string" },
                            { name : "devicefile", type : "string" },
                            { name : "description", type : "string" }
                        ]
                    }),
                    proxy : {
                        type : "rpc",
                        rpcData : {
                            service : "Subsonic",
                            method  : "getCandidates"
                        },
                        appendSortParams : false
                    },
                    sorters : [{
                        direction : "ASC",
                        property  : "devicefile"
                    }]
                })
            },{
                xtype      : "textfield",
                name       : "path",
                fieldLabel : _("Path"),
                allowNone  : true,
                readOnly   : true
            },{
                xtype   : "button",
                name    : "backup",
                text    : _("Backup"),
                scope   : this,
                handler : Ext.Function.bind(me.onBackupButton, me, [ me ]),
                margin  : "5 0 0 0"
            },{
                border : false,
                html   : "<ul><li>" + _("Backup settings to a data drive.") + "</li></ul>"
            },{
                xtype   : "button",
                name    : "restore",
                text    : _("Restore"),
                scope   : this,
                handler : Ext.Function.bind(me.onRestoreButton, me, [ me ]),
                margin  : "5 0 0 0"
            },{
                border : false,
                html   : "<ul><li>" + _("Restore settings from a data drive.") + "</li></ul>"
            }]
        }];
    },
	
	onBackupButton: function() {
        var me = this;
        me.doSubmit();
        Ext.create("OMV.window.Execute", {
            title      : _("Backup"),
            rpcService : "Subsonic",
            rpcMethod  : "doBackup",
            listeners  : {
                scope     : me,
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                }
            }
        }).show();
    },

	onRestoreButton: function() {
        var me = this;
        me.doSubmit();
        Ext.create("OMV.window.Execute", {
            title      : _("Restore"),
            rpcService : "Subsonic",
            rpcMethod  : "doRestore",
            listeners  : {
                scope     : me,
                exception : function(wnd, error) {
                    OMV.MessageBox.error(null, error);
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
