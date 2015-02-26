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
// require("js/omv/workspace/panel/Panel.js")

Ext.define("OMV.module.admin.service.subsonic.WebInterface", {
    extend : "OMV.workspace.panel.Panel",

    initComponent : function() {
        OMV.Rpc.request({
            scope    : this,
            callback : function(id, success, response) {
                var proxy = response.ppass;
                if (proxy == true) {
                    var link = "http://" + window.location.hostname + "/subsonic/";
                } else {
                    var link = "http://" + window.location.hostname + ":" + response.port;
				}
                this.html = "<iframe src='" + link + "' sandbox='allow-same-origin allow-forms allow-scripts' width='100%' height='100%' />";
            },
            relayErrors : false,
            rpcData     : {
                service  : "Subsonic",
                method   : "getSettings"
            }
        });

        this.callParent(arguments);
    }
});

OMV.WorkspaceManager.registerPanel({
    id        : "webinterface",
    path      : "/service/subsonic",
    text      : _("Web Interface"),
    position  : 20,
    className : "OMV.module.admin.service.subsonic.WebInterface"
});
