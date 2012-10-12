/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		<AUTHOR NAME HERE>
 */

#include "COM_SkeletonOperation.h"

SkeletonOperation::SkeletonOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputOperation = NULL;
}
void SkeletonOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
}

void SkeletonOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}


void SkeletonOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	this->m_inputOperation->read(output, x, y, sampler);
}

bool SkeletonOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
