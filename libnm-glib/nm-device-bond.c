/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * libnm_glib -- Access network status & information from glib applications
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2007 - 2008 Novell, Inc.
 * Copyright (C) 2007 - 2011 Red Hat, Inc.
 */

#include <config.h>
#include <string.h>
#include <netinet/ether.h>

#include <nm-setting-connection.h>
#include <nm-setting-bond.h>
#include <nm-utils.h>

#include "nm-device-bond.h"
#include "nm-device-private.h"
#include "nm-object-private.h"

G_DEFINE_TYPE (NMDeviceBond, nm_device_bond, NM_TYPE_DEVICE)

#define NM_DEVICE_BOND_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE_BOND, NMDeviceBondPrivate))

typedef struct {
	DBusGProxy *proxy;

	char *hw_address;
	gboolean carrier;
} NMDeviceBondPrivate;

enum {
	PROP_0,
	PROP_HW_ADDRESS,
	PROP_CARRIER,

	LAST_PROP
};

#define DBUS_PROP_HW_ADDRESS "HwAddress"
#define DBUS_PROP_CARRIER "Carrier"

/**
 * nm_device_bond_new:
 * @connection: the #DBusGConnection
 * @path: the DBus object path of the device
 *
 * Creates a new #NMDeviceBond.
 *
 * Returns: (transfer full): a new device
 **/
GObject *
nm_device_bond_new (DBusGConnection *connection, const char *path)
{
	GObject *device;

	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	device = g_object_new (NM_TYPE_DEVICE_BOND,
			       NM_OBJECT_DBUS_CONNECTION, connection,
			       NM_OBJECT_DBUS_PATH, path,
			       NULL);
	_nm_object_ensure_inited (NM_OBJECT (device));
	return device;
}

/**
 * nm_device_bond_get_hw_address:
 * @device: a #NMDeviceBond
 *
 * Gets the hardware (MAC) address of the #NMDeviceBond
 *
 * Returns: the hardware address. This is the internal string used by the
 * device, and must not be modified.
 **/
const char *
nm_device_bond_get_hw_address (NMDeviceBond *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_BOND (device), NULL);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_BOND_GET_PRIVATE (device)->hw_address;
}

/**
 * nm_device_bond_get_carrier:
 * @device: a #NMDeviceBond
 *
 * Whether the device has carrier.
 *
 * Returns: %TRUE if the device has carrier
 **/
gboolean
nm_device_bond_get_carrier (NMDeviceBond *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_BOND (device), FALSE);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_BOND_GET_PRIVATE (device)->carrier;
}

static gboolean
connection_valid (NMDevice *device, NMConnection *connection)
{
	NMSettingConnection *s_con;
	NMSettingBond *s_bond;
	const char *ctype, *dev_iface_name, *bond_iface_name;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	ctype = nm_setting_connection_get_connection_type (s_con);
	if (strcmp (ctype, NM_SETTING_BOND_SETTING_NAME) != 0)
		return FALSE;

	s_bond = nm_connection_get_setting_bond (connection);
	if (!s_bond)
		return FALSE;

	dev_iface_name = nm_device_get_iface (device);
	bond_iface_name = nm_setting_bond_get_interface_name (s_bond);
	if (g_strcmp0 (dev_iface_name, bond_iface_name) != 0)
		return FALSE;

	/* FIXME: check slaves? But we can't... */

	return TRUE;
}

/***********************************************************/

static void
nm_device_bond_init (NMDeviceBond *device)
{
	nm_device_set_device_type (NM_DEVICE (device), NM_DEVICE_TYPE_BOND);
}

static void
register_properties (NMDeviceBond *device)
{
	NMDeviceBondPrivate *priv = NM_DEVICE_BOND_GET_PRIVATE (device);
	const NMPropertiesInfo property_info[] = {
		{ NM_DEVICE_BOND_HW_ADDRESS, &priv->hw_address },
		{ NM_DEVICE_BOND_CARRIER,    &priv->carrier },
		{ NULL },
	};

	_nm_object_register_properties (NM_OBJECT (device),
	                                priv->proxy,
	                                property_info);
}

static void
constructed (GObject *object)
{
	NMDeviceBondPrivate *priv;

	G_OBJECT_CLASS (nm_device_bond_parent_class)->constructed (object);

	priv = NM_DEVICE_BOND_GET_PRIVATE (object);

	priv->proxy = dbus_g_proxy_new_for_name (nm_object_get_connection (NM_OBJECT (object)),
	                                         NM_DBUS_SERVICE,
	                                         nm_object_get_path (NM_OBJECT (object)),
	                                         NM_DBUS_INTERFACE_DEVICE_BOND);

	register_properties (NM_DEVICE_BOND (object));
}

static void
dispose (GObject *object)
{
	NMDeviceBondPrivate *priv = NM_DEVICE_BOND_GET_PRIVATE (object);

	if (priv->proxy) {
		g_object_unref (priv->proxy);
		priv->proxy = NULL;
	}

	G_OBJECT_CLASS (nm_device_bond_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMDeviceBondPrivate *priv = NM_DEVICE_BOND_GET_PRIVATE (object);

	g_free (priv->hw_address);

	G_OBJECT_CLASS (nm_device_bond_parent_class)->finalize (object);
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
	NMDeviceBond *device = NM_DEVICE_BOND (object);

	switch (prop_id) {
	case PROP_HW_ADDRESS:
		g_value_set_string (value, nm_device_bond_get_hw_address (device));
		break;
	case PROP_CARRIER:
		g_value_set_boolean (value, nm_device_bond_get_carrier (device));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_device_bond_class_init (NMDeviceBondClass *eth_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (eth_class);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (eth_class);

	g_type_class_add_private (eth_class, sizeof (NMDeviceBondPrivate));

	/* virtual methods */
	object_class->constructed = constructed;
	object_class->dispose = dispose;
	object_class->finalize = finalize;
	object_class->get_property = get_property;
	device_class->connection_valid = connection_valid;

	/* properties */

	/**
	 * NMDeviceBond:hw-address:
	 *
	 * The hardware (MAC) address of the device.
	 **/
	g_object_class_install_property
		(object_class, PROP_HW_ADDRESS,
		 g_param_spec_string (NM_DEVICE_BOND_HW_ADDRESS,
		                      "Active MAC Address",
		                      "Currently set hardware MAC address",
		                      NULL,
		                      G_PARAM_READABLE));

	/**
	 * NMDeviceBond:carrier:
	 *
	 * Whether the device has carrier.
	 **/
	g_object_class_install_property
		(object_class, PROP_CARRIER,
		 g_param_spec_boolean (NM_DEVICE_BOND_CARRIER,
		                       "Carrier",
		                       "Carrier",
		                       FALSE,
		                       G_PARAM_READABLE));

}
