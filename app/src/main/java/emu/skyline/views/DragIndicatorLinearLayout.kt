/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.views

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import androidx.core.content.getSystemService
import com.google.android.material.bottomsheet.BottomSheetBehavior
import emu.skyline.databinding.DragIndicatorBinding

class DragIndicatorLinearLayout : LinearLayout {
    private lateinit var binding : DragIndicatorBinding
    val callback = DragIndicatorCallback()

    init {
        orientation = VERTICAL
        addView(context.getSystemService<LayoutInflater>()?.run { DragIndicatorBinding.inflate(this).also { binding = it }.root })
    }

    constructor(context : Context) : super(context)
    constructor(context : Context, attrs : AttributeSet?) : super(context, attrs)
    constructor(context : Context, attrs : AttributeSet?, defStyleAttr : Int) : super(context, attrs, defStyleAttr)
    constructor(context : Context, attrs : AttributeSet?, defStyleAttr : Int, defStyleRes : Int) : super(context, attrs, defStyleAttr, defStyleRes)

    inner class DragIndicatorCallback : BottomSheetBehavior.BottomSheetCallback() {
        override fun onStateChanged(bottomSheet : View, newState : Int) {
            if (newState == BottomSheetBehavior.STATE_EXPANDED)
                binding.dragIndicator.visibility = View.INVISIBLE
            else
                binding.dragIndicator.visibility = View.VISIBLE
        }

        override fun onSlide(bottomSheet : View, slideOffset : Float) {}
    }
}
