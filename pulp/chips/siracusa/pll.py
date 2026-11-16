#
# SPDX-License-Identifier: Apache-2.0
#

import gvsoc.systree as st


class Pll(st.Component):

    def __init__(self, parent, name):
        super(Pll, self).__init__(parent, name)

        self.set_component('pulp.chips.siracusa.pll_impl')
