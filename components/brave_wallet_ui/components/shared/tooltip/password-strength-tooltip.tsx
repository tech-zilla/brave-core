// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

import * as React from 'react'

// utils
import { getLocale } from '$web-common/locale'

// components
import Tooltip from '.'

// style
import {
  PasswordStrengthText,
  PasswordStrengthTextWrapper,
  PasswordStrengthHeading
} from './password-strength-tooltip.style'

// types
import { PasswordStrengthResults } from '../../../common/hooks/use-password-strength'
import { GreenCheckmark } from '../style'

interface Props {
  isVisible: boolean
  passwordStrength: PasswordStrengthResults
  criteria: boolean[]
}

const PasswordStrengthDetails = ({
  passwordStrength: {
    containsNumber,
    containsSpecialChar,
    isLongEnough
  }
}: {
  passwordStrength: PasswordStrengthResults
}) => {
  return (
    <PasswordStrengthTextWrapper>
      <PasswordStrengthHeading>
        {getLocale('braveWalletPasswordStrengthTooltipHeading')}
      </PasswordStrengthHeading>

      <PasswordStrengthText isStrong={isLongEnough}>
        {isLongEnough && <GreenCheckmark />}{' '}
        {getLocale('braveWalletPasswordStrengthTooltipIsLongEnough')}
      </PasswordStrengthText>

      <PasswordStrengthText isStrong={containsNumber}>
        {containsNumber && <GreenCheckmark />}{' '}
        {getLocale('braveWalletPasswordStrengthTooltipContainsNumber')}
      </PasswordStrengthText>

      <PasswordStrengthText isStrong={containsSpecialChar}>
        {containsSpecialChar && <GreenCheckmark />}{' '}
        {getLocale('braveWalletPasswordStrengthTooltipContainsSpecialChar')}
      </PasswordStrengthText>

    </PasswordStrengthTextWrapper>
  )
}

export const PasswordStrengthTooltip: React.FC<React.PropsWithChildren<Props>> = ({
  children,
  isVisible,
  passwordStrength,
  criteria
}) => {
  return (
    <Tooltip
      disableHoverEvents
      verticalPosition='below'
      isVisible={isVisible}
      position='right'
      pointerPosition={'center'}
      text={<PasswordStrengthDetails passwordStrength={passwordStrength} />}
    >
      {children}
    </Tooltip>
  )
}

export default PasswordStrengthTooltip
