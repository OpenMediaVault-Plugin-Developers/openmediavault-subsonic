<?php

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

require_once "openmediavault/object.inc";
require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/util.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/notify.inc";
require_once("openmediavault/product.inc");
require_once("openmediavault/system.inc");

class OMVRpcServiceSubsonic extends OMVRpcServiceAbstract {

    public function getName() {
        return "Subsonic";
    }

    private function getPluginName() {
        return strtolower($this->getName());
    }

    private function getEventMessagePath() {
        $path = sprintf("org.openmediavault.service.%s", $this->getPluginName());
        return $path;
    }

    private function getXPath() {
        $path = sprintf("//services/%s", $this->getPluginName());
        return $path;
    }

    public function startService() {
        global $xmlConfig;

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath());

        if (!boolval($object["enable"]))
            return;

        $initd = new OMVSysVInitScript($this->getPluginName(), true);
        $initd->exec();
    }

    public function stopService() {
        $initd = new OMVSysVInitScript($this->getPluginName());
        $initd->exec();
    }

    public function initialize() {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod('doUpdateSUB');
        $this->registerMethod('doUpdateSUBB');

        $this->registerMethod('doBackup');
        $this->registerMethod('doRestore');
        $this->registerMethod('getCandidates');
    }

    function getSettings($params, $context) {
        $this->exec("omv-mkconf subsonic getsettings");
        global $xmlConfig;

        $this->validateMethodContext($context, array( "role" => OMV_ROLE_ADMINISTRATOR ));

        $object = $xmlConfig->get($this->getXPath());
        if(is_null($object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);

        $object["enable"]  = boolval($object["enable"]);
        $object["showtab"] = boolval($object["showtab"]);
        $object["update"]  = boolval($object["update"]);
        $object["bupdate"] = boolval($object["bupdate"]);
        $object['port']    = intval($object['port']);

        return $object;
    }

    function setSettings($params, $context) {
        global $xmlConfig;

        $this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));

        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "enable"    : { "type" : "boolean" },
                "showtab"   : { "type" : "boolean" },
                "update"    : { "type" : "boolean" },
                "bupdate"   : { "type" : "boolean" },
                "port"      : { "type" : "integer","minimum":1024,"maximum":65535},
                "mntentref" : { ' . $GLOBALS['OMV_JSONSCHEMA_UUID_NONE'] . ' },
                "msg"       : { "type" : "string", "optional" : true },
                "path"      : { "type" : "string", "optional" : true }
            }
        }');


        $xpath = sprintf("//system/fstab/mntent[uuid='%s']", $params['mntentref']);
        $deviceDir = $xmlConfig->get($xpath);
        $deviceDir = $deviceDir['dir'];


        $backupPath = "{$deviceDir}/SUBbackup";
        $params['path'] = $backupPath;


        // Get existing configuration object
        $oldObject = $xmlConfig->get($this->getXPath());
        if (is_null($oldObject))
           throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);

        $object = array(
            "enable"    => array_boolval($params, "enable"),
            "showtab"   => array_boolval($params, "showtab"),
            "update"    => array_boolval($params, "update"),
            "bupdate"   => array_boolval($params, "bupdate"),
            "port"      => $params["port"],
            "msg"       => $params["msg"],
            "mntentref" => $params["mntentref"],
            "path"      => $params["path"],
        );

        if(false === $xmlConfig->replace($this->getXPath(), $object))
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED, $this->getXPath());

        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePath(), $object);

        return $object;
    }

    public function getCandidates($params, $context) {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array( "role" => OMV_ROLE_ADMINISTRATOR ));

        // Get a list of mount points, except bind mounts.
        $xpath = "//system/fstab/mntent[not(contains(opts,'bind'))]";
        $objects = $xmlConfig->getList($xpath);
        if(is_null($objects)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        // Get the filesystem details for each mount point.
        $result = array();
        foreach($objects as $objectk => $objectv) {
            $fs = new OMVFilesystem($objectv['fsname']);
            if(!$fs->exists())
                continue;

            // Skip the filesystems that are not mounted at the moment.
            if(FALSE === $fs->isMounted())
                continue;

            // Get some filesystem details.
            $devicefile = $fs->getDeviceFile();
            $label = $fs->getLabel();

            // Prepare result object.
            $object = array(
                "uuid"        => $objectv['uuid'],
                "devicefile"  => $devicefile,
                "description" => !empty($label) ? $label : $devicefile
            );

            // Get some more filesystem details if possible.
            if(FALSE !== ($fsStats = $fs->getStatistics())) {
                $object['description'] = sprintf(
                    gettext("%s (%s available)"), $object['description'],
                    binary_format($fsStats['available']));
            }

            $result[] = $object;
        }
        return $result;
    }

    public function doBackup($params, $context) {
        global $xmlConfig;
		
        if (! file_exists("/var/subsonic/subsonic.properties"))
            throw new OMVException(OMVErrorMsg::E_EXEC_MISC, "No settings folder found!");

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array( "role" => OMV_ROLE_ADMINISTRATOR ));

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $path = $xmlConfig->get("//services/subsonic/path");

            $cmd = sprintf("tar -zcvf %s /var/subsonic/subsonic.properties /var/subsonic/db 2>&1", $path);
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function doRestore($params, $context) {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array( "role" => OMV_ROLE_ADMINISTRATOR ));

        // Get the configuration object.
        $path = $xmlConfig->get("//services/subsonic/path");
        $dirPath = "/var/subsonic";
        if(is_null($path)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        // Check if backup is present.
        if (file_exists($path)) {
            // Create a background process.
            $bgStatusFilename = $this->createBgProcStatus();
            $pid = $this->fork();
            if($pid > 0) { // Parent process.
                $this->initializeBgProcStatus($bgStatusFilename, $pid);
                return $bgStatusFilename;
            }

            // Delete current folder
            system("rm -rf ".escapeshellarg($dirPath));


            try {
                $this->stopService();
                $bgOutputFilename = $this->createBgProcOutput();
                $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

                $cmd = sprintf("tar -zxvf %s -C / 2>&1", $path);

                $this->exec($cmd, $output, $bgOutputFilename);
                $this->finalizeBgProcStatus($bgStatusFilename, $output);
                $this->startService();
                exit(0);
            } catch(Exception $e) {
                $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
                $this->startService();
                exit(1);
            }
        } else {
            throw new OMVException(OMVErrorMsg::E_EXEC_MISC, "No backup found!");
        }
    }

    public function doUpdateSUB($params, $context) {
        $cmd = "omv-mkconf subsonic update 2>&1";
        if(0 !== $this->exec($cmd, $output))
           throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePath(), $object);
    }

    public function doUpdateSUBB($params, $context) {
        $cmd = "omv-mkconf subsonic bupdate 2>&1";
        if(0 !== $this->exec($cmd, $output))
           throw new OMVException(OMVErrorMsg::E_EXEC_FAILED, $cmd, implode("\n", $output));
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePath(), $object);
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceSubsonic());

