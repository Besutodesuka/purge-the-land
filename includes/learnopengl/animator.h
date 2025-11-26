#pragma once

#include <glm/glm.hpp>
#include <map>
#include <vector>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <learnopengl/animation.h>
#include <learnopengl/bone.h>

class Animator
{
public:
	Animator(Animation* animation)
	{
		m_CurrentTime = 0.0;
		m_CurrentTime2 = 0.0;
		m_CurrentAnimation = animation;
		m_CurrentAnimation2 = NULL;
		m_blendAmount = 0.0f;

		m_FinalBoneMatrices.reserve(100);

		for (int i = 0; i < 100; i++)
			m_FinalBoneMatrices.push_back(glm::mat4(1.0f));
	}

	void UpdateAnimation(float dt)
	{
		m_DeltaTime = dt;
		if (m_CurrentAnimation)
		{
            // Update time for the primary animation
			m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * dt;
			m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());

            // Update time for the secondary animation only if it's active
			if (m_CurrentAnimation2)
			{
				m_CurrentTime2 += m_CurrentAnimation2->GetTicksPerSecond() * dt;
				m_CurrentTime2 = fmod(m_CurrentTime2, m_CurrentAnimation2->GetDuration());
			}

            // Calculate bone transforms for the current frame (will handle blend internally)
			CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
		}
	}

	void PlayAnimation(Animation* pAnimation, Animation* pAnimation2, float time1, float time2, float blend)
	{
		m_CurrentAnimation = pAnimation;
		m_CurrentTime = time1;
		m_CurrentAnimation2 = pAnimation2;
		m_CurrentTime2 = time2;
		m_blendAmount = blend;
	}

    // UpdateBlend now uses the Bone's public GetInterpolatedX methods
	glm::mat4 UpdateBlend(Bone* Bone1, Bone* Bone2, float time1, float time2, float blend) {
		// Get interpolated components directly from each bone
		glm::vec3 bonePos1 = Bone1->GetInterpolatedPosition(time1);
		glm::vec3 bonePos2 = Bone2->GetInterpolatedPosition(time2);
		glm::quat boneRot1 = Bone1->GetInterpolatedRotation(time1);
		glm::quat boneRot2 = Bone2->GetInterpolatedRotation(time2);
		glm::vec3 boneScale1 = Bone1->GetInterpolatedScaling(time1);
		glm::vec3 boneScale2 = Bone2->GetInterpolatedScaling(time2);

		// Mix the components
		glm::vec3 finalPos = glm::mix(bonePos1, bonePos2, blend);
		glm::quat finalRot = glm::slerp(boneRot1, boneRot2, blend);
		finalRot = glm::normalize(finalRot);
		glm::vec3 finalScale = glm::mix(boneScale1, boneScale2, blend);

		// Combine into a single matrix
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), finalPos);
		glm::mat4 rotation = glm::toMat4(finalRot);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), finalScale);

		return translation * rotation * scale;
	}

	void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
	{
		std::string nodeName = node->name;
		// Initialize nodeTransform with the node's original bind pose transform.
        // This is important for bones that might not be animated in *any* current animation.
		glm::mat4 nodeTransform = node->transformation;

		Bone* Bone1 = m_CurrentAnimation->FindBone(nodeName);
		Bone* Bone2 = nullptr;

		// Only look for Bone2 if a secondary animation is set and blending is active
		if (m_CurrentAnimation2 && m_blendAmount > 0.0f) {
			Bone2 = m_CurrentAnimation2->FindBone(nodeName);
		}

		if (Bone1) // If the bone is animated in the primary animation
		{
            // If we have a second animation bone AND it's found AND we are blending
			if (Bone2 && m_blendAmount > 0.0f)
			{
				// Calculate a blended transformation for this bone
				nodeTransform = UpdateBlend(Bone1, Bone2, m_CurrentTime, m_CurrentTime2, m_blendAmount);
			}
			else // No blending for this bone (either no second animation, or blendAmount is 0)
			{
				// Use the primary animation's transform directly
				nodeTransform = Bone1->GetAnimatedTransform(m_CurrentTime);
			}
		}
        // If Bone1 is nullptr, nodeTransform remains its bind pose, which is correct
        // for un-animated bones.

		glm::mat4 globalTransformation = parentTransform * nodeTransform;

		auto boneInfoMap = m_CurrentAnimation->GetBoneIDMap();
		if (boneInfoMap.find(nodeName) != boneInfoMap.end())
		{
			int index = boneInfoMap[nodeName].id;
			glm::mat4 offset = boneInfoMap[nodeName].offset;
			m_FinalBoneMatrices[index] = globalTransformation * offset;
		}

		for (int i = 0; i < node->childrenCount; i++)
			CalculateBoneTransform(&node->children[i], globalTransformation);
	}

	std::vector<glm::mat4> GetFinalBoneMatrices()
	{
		return m_FinalBoneMatrices;
	}

public: // Keep these public for skeletal_animation.cpp state machine
	std::vector<glm::mat4> m_FinalBoneMatrices;
	Animation* m_CurrentAnimation;
	Animation* m_CurrentAnimation2;
	float m_CurrentTime;
	float m_CurrentTime2;
	float m_DeltaTime;
	float m_blendAmount;

};