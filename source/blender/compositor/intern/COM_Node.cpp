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
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include <string.h>

#include "BKE_node.h"

#include "COM_Node.h"
#include "COM_NodeOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketConnection.h"
#include "COM_ExecutionSystem.h"
#include "COM_PreviewOperation.h"
#include "COM_TranslateOperation.h"

#include "COM_SocketProxyNode.h"

//#include <stdio.h>
#include "COM_defines.h"

Node::Node(bNode *editorNode, bool create_sockets): NodeBase()
{
	setbNode(editorNode);
	
	if (create_sockets) {
		bNodeSocket *input = (bNodeSocket *)editorNode->inputs.first;
		while (input != NULL) {
			DataType dt = COM_DT_VALUE;
			if (input->type == SOCK_RGBA) dt = COM_DT_COLOR;
			if (input->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
			
			this->addInputSocket(dt, (InputSocketResizeMode)input->resizemode, input);
			input = (bNodeSocket *)input->next;
		}
		bNodeSocket *output = (bNodeSocket *)editorNode->outputs.first;
		while (output != NULL) {
			DataType dt = COM_DT_VALUE;
			if (output->type == SOCK_RGBA) dt = COM_DT_COLOR;
			if (output->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
			
			this->addOutputSocket(dt, output);
			output = (bNodeSocket *)output->next;
		}
	}
}

void Node::addSetValueOperation(ExecutionSystem *graph, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getEditorInputSocket(editorNodeInputSocketIndex);
	SetValueOperation *operation = new SetValueOperation();
	bNodeSocketValueFloat *val = (bNodeSocketValueFloat *)bSock->default_value;
	operation->setValue(val->value);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
	graph->addOperation(operation);
}

void Node::addPreviewOperation(ExecutionSystem *system, CompositorContext *context, OutputSocket *outputSocket)
{
	if (this->isInActiveGroup()) {
		if (!(this->getbNode()->flag & NODE_HIDDEN)) { // do not calculate previews of hidden nodes.
			if (this->getbNode()->flag & NODE_PREVIEW) {
				PreviewOperation *operation = new PreviewOperation(context->getViewSettings(), context->getDisplaySettings());
				system->addOperation(operation);
				operation->setbNode(this->getbNode());
				operation->setbNodeTree(system->getContext().getbNodeTree());
				this->addLink(system, outputSocket, operation->getInputSocket(0));
			}
		}
	}
}

void Node::addPreviewOperation(ExecutionSystem *system, CompositorContext *context, InputSocket *inputSocket)
{
	if (inputSocket->isConnected() && this->isInActiveGroup()) {
		OutputSocket *outputsocket = inputSocket->getConnection()->getFromSocket();
		this->addPreviewOperation(system, context, outputsocket);
	}
}

SocketConnection *Node::addLink(ExecutionSystem *graph, OutputSocket *outputSocket, InputSocket *inputsocket)
{
	if (inputsocket->isConnected()) {
		return NULL;
	}
	SocketConnection *connection = new SocketConnection();
	connection->setFromSocket(outputSocket);
	outputSocket->addConnection(connection);
	connection->setToSocket(inputsocket);
	inputsocket->setConnection(connection);
	graph->addSocketConnection(connection);
	return connection;
}

void Node::addSetColorOperation(ExecutionSystem *graph, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getEditorInputSocket(editorNodeInputSocketIndex);
	SetColorOperation *operation = new SetColorOperation();
	bNodeSocketValueRGBA *val = (bNodeSocketValueRGBA *)bSock->default_value;
	operation->setChannel1(val->value[0]);
	operation->setChannel2(val->value[1]);
	operation->setChannel3(val->value[2]);
	operation->setChannel4(val->value[3]);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
	graph->addOperation(operation);
}

void Node::addSetVectorOperation(ExecutionSystem *graph, InputSocket *inputsocket, int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getEditorInputSocket(editorNodeInputSocketIndex);
	bNodeSocketValueVector *val = (bNodeSocketValueVector *)bSock->default_value;
	SetVectorOperation *operation = new SetVectorOperation();
	operation->setX(val->value[0]);
	operation->setY(val->value[1]);
	operation->setZ(val->value[2]);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
	graph->addOperation(operation);
}

/* when a node has no valid data (missing image or group pointer) */
void Node::convertToOperations_invalid(ExecutionSystem *graph, CompositorContext *context)
{
	/* this is a really bad situation - bring on the pink! - so artists know this is bad */
	const float warning_color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
	int index;
	vector<OutputSocket *> &outputsockets = this->getOutputSockets();
	for (index = 0; index < outputsockets.size(); index++) {
		SetColorOperation *operation = new SetColorOperation();
		this->getOutputSocket(index)->relinkConnections(operation->getOutputSocket());
		operation->setChannels(warning_color);
		graph->addOperation(operation);
	}
}

bNodeSocket *Node::getEditorInputSocket(int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->inputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}
bNodeSocket *Node::getEditorOutputSocket(int editorNodeInputSocketIndex)
{
	bNodeSocket *bSock = (bNodeSocket *)this->getbNode()->outputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}

InputSocket *Node::findInputSocketBybNodeSocket(bNodeSocket *socket)
{
	vector<InputSocket *> &inputsockets = this->getInputSockets();
	unsigned int index;
	for (index = 0; index < inputsockets.size(); index++) {
		InputSocket *input = inputsockets[index];
		if (input->getbNodeSocket() == socket) {
			return input;
		}
	}
	return NULL;
}

OutputSocket *Node::findOutputSocketBybNodeSocket(bNodeSocket *socket)
{
	vector<OutputSocket *> &outputsockets = this->getOutputSockets();
	unsigned int index;
	for (index = 0; index < outputsockets.size(); index++) {
		OutputSocket *output = outputsockets[index];
		if (output->getbNodeSocket() == socket) {
			return output;
		}
	}
	return NULL;
}
