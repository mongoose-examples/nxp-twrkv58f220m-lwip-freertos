/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_phyksz8041.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief Defines the timeout macro. */
#define PHY_TIMEOUT_COUNT 100000

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

const phy_operations_t phyksz8041_ops = {.phyInit            = PHY_KSZ8041_Init,
                                         .phyWrite           = PHY_KSZ8041_Write,
                                         .phyRead            = PHY_KSZ8041_Read,
                                         .getLinkStatus      = PHY_KSZ8041_GetLinkStatus,
                                         .getLinkSpeedDuplex = PHY_KSZ8041_GetLinkSpeedDuplex,
                                         .enableLoopback     = PHY_KSZ8041_EnableLoopback};

/*******************************************************************************
 * Code
 ******************************************************************************/

status_t PHY_KSZ8041_Init(phy_handle_t *handle, const phy_config_t *config)
{
    uint32_t bssReg;
    uint32_t counter = PHY_TIMEOUT_COUNT;
    uint32_t idReg   = 0;
    status_t result  = kStatus_Success;

    /* Init MDIO interface. */
    MDIO_Init(handle->mdioHandle);

    /* assign phy address. */
    handle->phyAddr = config->phyAddr;

    /* Initialization after PHY stars to work. */
    while ((idReg != PHY_CONTROL_ID1) && (counter != 0))
    {
        MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_ID1_REG, &idReg);
        counter--;
    }

    if (!counter)
    {
        return kStatus_Fail;
    }

    /* Reset PHY. */
    counter = PHY_TIMEOUT_COUNT;
    result  = MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG, PHY_BCTL_RESET_MASK);
    if (result == kStatus_Success)
    {
        /* Set the negotiation. */
        result = MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_AUTONEG_ADVERTISE_REG,
                            (PHY_100BASETX_FULLDUPLEX_MASK | PHY_100BASETX_HALFDUPLEX_MASK |
                             PHY_10BASETX_FULLDUPLEX_MASK | PHY_10BASETX_HALFDUPLEX_MASK | 0x1U));
        if (result == kStatus_Success)
        {
            result = MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG,
                                (PHY_BCTL_AUTONEG_MASK | PHY_BCTL_RESTART_AUTONEG_MASK));
            if (result == kStatus_Success)
            {
                /* Check auto negotiation complete. */
                while (counter--)
                {
                    result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_BASICSTATUS_REG, &bssReg);
                    if (result == kStatus_Success)
                    {
                        if ((bssReg & PHY_BSTATUS_AUTONEGCOMP_MASK) != 0)
                        {
                            break;
                        }
                    }

                    if (!counter)
                    {
                        return kStatus_PHY_AutoNegotiateFail;
                    }
                }
            }
        }
    }

    return result;
}

status_t PHY_KSZ8041_Write(phy_handle_t *handle, uint32_t phyReg, uint32_t data)
{
    return MDIO_Write(handle->mdioHandle, handle->phyAddr, phyReg, data);
}

status_t PHY_KSZ8041_Read(phy_handle_t *handle, uint32_t phyReg, uint32_t *dataPtr)
{
    return MDIO_Read(handle->mdioHandle, handle->phyAddr, phyReg, dataPtr);
}

status_t PHY_KSZ8041_EnableLoopback(phy_handle_t *handle, phy_loop_t mode, phy_speed_t speed, bool enable)
{
    status_t result;
    uint32_t data = 0;

    /* Set the loop mode. */
    if (enable)
    {
        if (mode == kPHY_LocalLoop)
        {
            if (speed == kPHY_Speed100M)
            {
                data = PHY_BCTL_SPEED0_MASK | PHY_BCTL_DUPLEX_MASK | PHY_BCTL_LOOP_MASK;
            }
            else
            {
                data = PHY_BCTL_DUPLEX_MASK | PHY_BCTL_LOOP_MASK;
            }
            return MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG, data);
        }
        else
        {
            /* First read the current status in control register. */
            result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_CONTROL1_REG, &data);
            if (result == kStatus_Success)
            {
                return MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_CONTROL1_REG,
                                  (data | PHY_CTL1_REMOTELOOP_MASK));
            }
        }
    }
    else
    {
        /* Disable the loop mode. */
        if (mode == kPHY_LocalLoop)
        {
            /* First read the current status in control register. */
            result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG, &data);
            if (result == kStatus_Success)
            {
                data &= ~PHY_BCTL_LOOP_MASK;
                return MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_BASICCONTROL_REG,
                                  (data | PHY_BCTL_RESTART_AUTONEG_MASK));
            }
        }
        else
        {
            /* First read the current status in control one register. */
            result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_CONTROL1_REG, &data);
            if (result == kStatus_Success)
            {
                return MDIO_Write(handle->mdioHandle, handle->phyAddr, PHY_CONTROL1_REG,
                                  (data & ~PHY_CTL1_REMOTELOOP_MASK));
            }
        }
    }
    return result;
}

status_t PHY_KSZ8041_GetLinkStatus(phy_handle_t *handle, bool *status)
{
    assert(status);

    status_t result = kStatus_Success;
    uint32_t data;

    /* Read the basic status register. */
    result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_BASICSTATUS_REG, &data);
    if (result == kStatus_Success)
    {
        if (!(PHY_BSTATUS_LINKSTATUS_MASK & data))
        {
            /* link down. */
            *status = false;
        }
        else
        {
            /* link up. */
            *status = true;
        }
    }
    return result;
}

status_t PHY_KSZ8041_GetLinkSpeedDuplex(phy_handle_t *handle, phy_speed_t *speed, phy_duplex_t *duplex)
{
    assert(duplex);

    status_t result = kStatus_Success;
    uint32_t data, ctlReg;

    /* Read the control two register. */
    result = MDIO_Read(handle->mdioHandle, handle->phyAddr, PHY_CONTROL2_REG, &ctlReg);
    if (result == kStatus_Success)
    {
        data = ctlReg & PHY_BSTATUS_SPEEDUPLX_MASK;
        if ((PHY_CTL2_10FULLDUPLEX_MASK == data) || (PHY_CTL2_100FULLDUPLEX_MASK == data))
        {
            /* Full duplex. */
            *duplex = kPHY_FullDuplex;
        }
        else
        {
            /* Half duplex. */
            *duplex = kPHY_HalfDuplex;
        }

        data = ctlReg & PHY_BSTATUS_SPEEDUPLX_MASK;
        if ((PHY_CTL2_100HALFDUPLEX_MASK == data) || (PHY_CTL2_100FULLDUPLEX_MASK == data))
        {
            /* 100M speed. */
            *speed = kPHY_Speed100M;
        }
        else
        { /* 10M speed. */
            *speed = kPHY_Speed10M;
        }
    }

    return result;
}
