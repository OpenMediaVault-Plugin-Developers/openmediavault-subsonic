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
// require("js/omv/workspace/window/Form.js")

Ext.define("OMV.module.admin.service.subsonic.Backup", {
    extend: "OMV.workspace.window.Form",

    hideResetButton: true,
    hideOkButton: true,
    title: _("Backup/restore"),

    getFormItems: function() {
        return [{
            xtype: "sharedfoldercombo",
            name: "sharedfolderref",
            fieldLabel: _("Backup location"),
            allowBlank: false,
            allowNone: false
        }];
    },

    getButtonItems: function(c) {
        var items = this.callParent(arguments);

        Ext.Array.insert(items, 0, [{
            id: this.getId() + "-backup",
            xtype: "button",
            text: _("Backup"),
            handler: Ext.Function.bind(this.onBackupButton, this),
            scope: this
        }, {
            id: this.getId() + "-restore",
            xtype: "button",
            text: _("Restore"),
            handler: Ext.Function.bind(this.onRestoreButton, this),
            scope: this
        }]);

        return items;
    },

    onBackupButton: function() {
        if (!this.isValid()) {
            this.markInvalid();

            return;
        }

        this.doAction("doBackup");
    },

    onRestoreButton: function() {
        if (!this.isValid()) {
            this.markInvalid();

            return;
        }

        this.doAction("doRestore");
    },

    doAction: function(method) {
        var wnd = Ext.create("OMV.window.Execute", {
            title: _("Performing backup/restore ..."),
            rpcService: "Subsonic",
            rpcMethod: method,
            rpcParams: this.getRpcSetParams(),
            rpcIgnoreErrors: false,
            hideStartButton: true,
            hideStopButton: true,
            listeners: {
                scope: this,
                finish: function(wnd, response) {
                    wnd.appendValue(_("Done ..."));
                    wnd.setButtonDisabled("close", false);
                },
                exception: function(wnd, error) {
                    OMV.MessageBox.error(null, error);
                    wnd.setButtonDisabled("close", false);
                }
            }
        });

        wnd.setButtonDisabled("close", true);
        wnd.show();
        wnd.start();
    }
});
